/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Thread scheduler.
 *
 * The thread scheduler maintains per-CPU prioritized run queues. Internally,
 * there are 32 priority levels, which process priority classes and thread
 * priorities are mapped on to. Each CPU has two priority queues: active and
 * expired. A priority queue is an array of linked lists for each priority
 * level, and a bitmap of lists with queues in. This makes thread selection
 * O(1): find the highest set bit in the bitmap, and take the first thread off
 * that list.
 *
 * The active queue is used to select threads to run, and when a thread needs
 * to be reinserted into the queue after using all of its timeslice, it is
 * placed in the expired queue. When selecting a thread, if there are no threads
 * in the active queue, but there are in the expired queue, the active and
 * expired queues are swapped. This alleviates starvation of lower priority
 * threads by guaranteeing that all threads waiting in the active queue will be
 * given a chance to run before going back to higher priority threads that are
 * using up all of their timeslice.
 *
 * CPU-bound threads get punished by a small decrease in priority (a maximum of
 * 5) to ensure that interactive threads at the same priority get given more
 * chance to run. The current heuristic used to determine whether a thread is
 * CPU-bound is to see whether it uses up all of its timeslice.
 *
 * On multi-CPU systems, load is balanced between CPUs when moving threads into
 * the Ready state by picking an appropriate CPU to run on. Currently, the CPU
 * that the thread previously ran on is favoured if it is not heavily loaded.
 * Otherwise, the CPU with the lowest load is picked.
 *
 * @todo		Load balancing could be improved in situations where
 *			there are several CPU-bound threads running. Currently,
 *			a thread is only moved to another CPU when it is woken
 *			from sleep. This could be done by stealing threads from
 *			other CPUs when a CPU becomes idle.
 * @todo		Once the facility to get CPU topology information is
 *			implemented, we should be more friendly to HT systems
 *			when picking a CPU for a thread to run on by favouring
 *			idle logical CPUs on a core that is not already running
 *			a thread on another of its logical CPUs.
 * @todo		Possibly a better heuristic for determining whether a
 *			thread is CPU-/IO-bound is to look at how much time it
 *			spends sleeping.
 * @todo		Alter timeslice based on priority?
 */

#include <arch/bitops.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <kdbg.h>
#include <status.h>
#include <time.h>

#if CONFIG_SCHED_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Number of priority levels. */
#define PRIORITY_COUNT		32

/** Timeslice to give to threads. */
#define THREAD_TIMESLICE	MSECS2USECS(3)

/** Maximum penalty to CPU-bound threads. */
#define MAX_PENALTY		5

/** Set to 1 to enable a debug message for every thread switch. */
#define SCHED_OVERKILL_DEBUG	0

/** Run queue structure. */
typedef struct sched_queue {
	unative_t bitmap;		/**< Bitmap of queues with data. */
	list_t threads[PRIORITY_COUNT];	/**< Queues of runnable threads. */
} sched_queue_t;

/** Per-CPU scheduling information structure. */
typedef struct sched_cpu {
	spinlock_t lock;		/**< Lock to protect information/queues. */

	thread_t *prev_thread;		/**< Previously executed thread. */

	thread_t *idle_thread;		/**< Thread scheduled when no other threads runnable. */
	timer_t timer;			/**< Preemption timer. */
	sched_queue_t *active;		/**< Active queue. */
	sched_queue_t *expired;		/**< Expired queue. */
	sched_queue_t queues[2];	/**< Active and expired queues. */
	size_t total;			/**< Total running/ready thread count. */
} sched_cpu_t;

extern void sched_internal(bool state);
extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);

/** Total number of running/ready threads across all CPUs. */
static atomic_t threads_running = 0;

/** Add a thread to a queue.
 * @param queue		Queue to add to.
 * @param thread	Thread to add. */
static inline void sched_queue_insert(sched_queue_t *queue, thread_t *thread) {
	list_append(&queue->threads[thread->curr_prio], &thread->runq_link);
	queue->bitmap |= (1 << thread->curr_prio);
}

/** Remove a thread from a queue.
 * @param queue		Queue to remove from.
 * @param thread	Thread to remove. */
static inline void sched_queue_remove(sched_queue_t *queue, thread_t *thread) {
	list_remove(&thread->runq_link);
	if(list_empty(&queue->threads[thread->curr_prio])) {
		queue->bitmap &= ~(1 << thread->curr_prio);
	}
}

/** Calculate the initial priority of a thread.
 * @param thread	Thread to calculate for. */
static inline void sched_calculate_priority(thread_t *thread) {
	int priority;

	/* We need to map the priority class and the thread priority onto our
	 * 32 priority levels. See documentation/sched-priorities.txt for a
	 * pretty bad explanation of what this is doing. */
	priority = 5 + (thread->owner->priority * 8);
	priority += (thread->priority - 1) * 2;

	dprintf("sched: setting priority for thread %" PRId32 " to %d (proc: %d, thread: %d)\n",
	        thread->id, priority, thread->owner->priority, thread->priority);
	thread->max_prio = thread->curr_prio = priority;
}

/** Tweak priority of a thread being stored.
 * @param cpu		Per-CPU scheduler information structure.
 * @param thread	Thread to tweak. */
static inline void sched_tweak_priority(sched_cpu_t *cpu, thread_t *thread) {
	if(thread->timeslice != 0) {
		/* The timeslice wasn't fully used, give a bonus if we're not
		 * already at the maximum priority. */
		if(thread->curr_prio < thread->max_prio) {
			thread->curr_prio++;
			dprintf("sched: thread %" PRId32 " (%" PRId32 ") bonus (new: %d, max: %d)\n",
				thread->id, thread->owner->id, thread->curr_prio, thread->max_prio);
		}
	} else {
		/* The timeslice was used up, penalise the thread if we've not
		 * already given it the maximum penalty. */
		if(thread->curr_prio && (thread->max_prio - thread->curr_prio) < MAX_PENALTY) {
			thread->curr_prio--;
			dprintf("sched: thread %" PRId32 " (%" PRId32 ") penalty (new: %d, max: %d)\n",
				thread->id, thread->owner->id, thread->curr_prio, thread->max_prio);
		}
	}
}

/** Pick a new thread to run.
 * @param cpu		Per-CPU scheduler information structure.
 * @return		Pointer to thread, or NULL if no threads available. */
static thread_t *sched_cpu_pick(sched_cpu_t *cpu) {
	thread_t *thread;
	int i;

	if(!cpu->active->bitmap) {
		/* The active queue is empty. If there are threads on the
		 * expired queue, swap them. */
		if(cpu->expired->bitmap) {
			SWAP(cpu->active, cpu->expired);
		} else {
			return NULL;
		}
	}

	/* Get the thread to run. */
	i = bitops_fls(cpu->active->bitmap);
	thread = list_entry(cpu->active->threads[i].next, thread_t, runq_link);
	sched_queue_remove(cpu->active, thread);
	return thread;
}

/** Scheduler timer handler function.
 * @param data		Data argument (unused).
 * @return		Always returns true. */
static bool sched_timer_handler(void *data) {
	curr_thread->timeslice = 0;
	return true;
}

/** Internal part of the thread scheduler. Expects current thread to be locked.
 * @param state		Previous interrupt state. */
void sched_internal(bool state) {
	sched_cpu_t *cpu = curr_cpu->sched;
	thread_t *new;

	assert(!atomic_get(&kdbg_running));

	spinlock_lock_ni(&cpu->lock);

	/* Thread can't be in ready state if we're running it now. */
	assert(curr_thread->state != THREAD_READY);

	/* Tweak the priority of the thread based on whether it used up its
	 * timeslice. */
	if(curr_thread != cpu->idle_thread) {
		sched_tweak_priority(cpu, curr_thread);
	}

	if(curr_thread->state == THREAD_RUNNING) {
		/* The thread hasn't gone to sleep, re-queue it. */
		curr_thread->state = THREAD_READY;
		if(curr_thread != cpu->idle_thread) {
			sched_queue_insert(cpu->expired, curr_thread);
		}
	} else {
		/* Thread is no longer running, decrease counts. */
		assert(curr_thread != cpu->idle_thread);
		cpu->total--;
		atomic_dec(&threads_running);
	}

	/* Find a new thread to run. A NULL return value means no threads are
	 * ready, so we schedule the idle thread in this case. This will
	 * return with the new thread locked if it is not the current. */
	new = sched_cpu_pick(cpu);
	if(!new) {
		new = cpu->idle_thread;
		if(new != curr_thread) {
			spinlock_lock_ni(&new->lock);
			dprintf("sched: cpu %" PRIu32 " has no runnable threads remaining, idling\n", curr_cpu->id);
		}

		/* The idle thread runs indefinitely until an interrupt causes
		 * something to be woken up. */
		new->timeslice = 0;
		curr_cpu->idle = true;
	} else {
		if(new != curr_thread) {
			spinlock_lock_ni(&new->lock);
		}

		new->timeslice = THREAD_TIMESLICE;
		curr_cpu->idle = false;
	}

	/* Move the thread to the Running state and set it as the current. */
	cpu->prev_thread = curr_thread;
	new->state = THREAD_RUNNING;
	curr_thread = new;

	/* Finished with the scheduler queues, unlock. */
	spinlock_unlock_ni(&cpu->lock);

	/* Set off the timer if necessary. */
	if(curr_thread->timeslice > 0) {
		timer_start(&cpu->timer, curr_thread->timeslice, TIMER_ONESHOT);
	}

	/* Only need to continue if the new thread is different. */
	if(curr_thread != cpu->prev_thread) {
#if SCHED_OVERKILL_DEBUG
		kprintf(LOG_DEBUG, "sched: switching to thread %" PRId32 "(%s) (process: %" PRId32 ", cpu: %" PRIu32 ")\n",
			curr_thread->id, curr_thread->name, curr_proc->id, curr_cpu->id);
#endif

		/* The switch may return to thread_trampoline() or to the
		 * interruption handler in waitq_sleep_unsafe(), so put
		 * anything to do after a switch in sched_post_switch(). */
		if(curr_thread != cpu->prev_thread) {
			/* Switch the address space. If the new process' address
			 * space is set to NULL then vm_aspace_switch() will
			 * just switch to the kernel address space. */
			vm_aspace_switch(curr_proc->aspace);

			/* Save old FPU state if necessary, and disable the FPU.
			 * It will be re-enabled on-demand if required. */
			if(fpu_state()) {
				assert(cpu->prev_thread->fpu);
				fpu_context_save(cpu->prev_thread->fpu);
				fpu_disable();
			}

			/* Switch to the new CPU context. */
			if(!context_save(&cpu->prev_thread->context)) {
				context_restore(&curr_thread->context);
			}
		}

		sched_post_switch(state);
	} else {
		spinlock_unlock_ni(&curr_thread->lock);
		intr_restore(state);
	}
}

/** Perform post-thread-switch tasks.
 * @param state		Interrupt state to restore. */
void sched_post_switch(bool state) {
	/* Do architecture-specific post-switch tasks. */
	thread_arch_post_switch(curr_thread);

	spinlock_unlock_ni(&curr_thread->lock);

	/* The prev_thread pointer is set to NULL during sched_init(). It will
	 * only ever be NULL once. */
	if(likely(curr_cpu->sched->prev_thread)) {
		spinlock_unlock_ni(&curr_cpu->sched->prev_thread->lock);

		/* Deal with thread terminations. */
		if(curr_cpu->sched->prev_thread->state == THREAD_DEAD) {
			thread_destroy(curr_cpu->sched->prev_thread);
		}
	}

	intr_restore(state);
}

/** Allocate a CPU for a thread to run on.
 * @param thread	Thread to allocate for. */
static inline cpu_t *sched_allocate_cpu(thread_t *thread) {
	size_t total, average, load;
	cpu_t *cpu, *other;

	/* On uniprocessor systems, we only have one choice. */
	if(cpu_count == 1) {
		return curr_cpu;
	}

	/* Add 1 to the total number of threads to account for the thread we're
	 * adding. */
	total = atomic_get(&threads_running) + 1;

	/* Start on the current CPU that the thread currently belongs to. */
	cpu = (thread->cpu) ? thread->cpu : curr_cpu;

	/* Get the average number of threads that a CPU should have as well as
	 * the CPU's current load. We round up to a multiple of the CPU count
	 * rather than rounding down here to stop us from loading off threads
	 * unnecessarily. */
	average = (ROUND_UP(total, cpu_count) / cpu_count);
	load = cpu->sched->total;

	/* If the CPU has less than or equal to the average, we're OK to keep
	 * the thread on it. */
	if(load <= average) {
		return cpu;
	}

	/* We need to pick another CPU. Try to find one with a load less than
	 * the average. If there isn't one, we just keep the thread on its
	 * current CPU. */
	LIST_FOREACH(&cpus_running, iter) {
		other = list_entry(iter, cpu_t, header);

		load = other->sched->total;
		if(load < average) {
			dprintf("sched: CPU %u load %zu less than average %zu, giving it thread %u\n",
			        other->id, load, average, thread->id);
			cpu = other;
			break;
		}
	}

	return cpu;
}

/** Insert a thread into the scheduler.
 * @param thread	Thread to insert (should be locked). */
void sched_thread_insert(thread_t *thread) {
	sched_cpu_t *sched;

	assert(thread->state == THREAD_READY);

	/* If we've been newly created, we will not have a priority set.
	 * Calculate the maximum priority for the thread. */
	if(unlikely(thread->max_prio < 0)) {
		sched_calculate_priority(thread);
	}

	if(thread->wire_count) {
		/* If the wire count is greater than 0 and the thread doesn't
		 * have a CPU, it means that it was wired before it started
		 * running. Put it onto the current CPU in this case. */
		if(unlikely(!thread->cpu)) {
			thread->cpu = curr_cpu;
		}
	} else {
		/* Pick a new CPU for the thread to run on. */
		thread->cpu = sched_allocate_cpu(thread);
	}

	sched = thread->cpu->sched;
	spinlock_lock(&sched->lock);

	sched_queue_insert(sched->active, thread);
	sched->total++;
	atomic_inc(&threads_running);

	/* If the thread has a higher priority than the currently running
	 * thread on the CPU, or if the CPU is idle, preempt it. */
	if(thread->cpu->idle || thread->curr_prio > thread->cpu->thread->curr_prio) {
		if(!thread->cpu->idle) {
			thread->cpu->should_preempt = true;
		}
		if(thread->cpu != curr_cpu) {
			ipi_send(thread->cpu->id, NULL, 0, 0, 0, 0, 0);
		}
	}

	spinlock_unlock(&sched->lock);
}

/** Yield remaining timeslice and switch to another thread. */
void sched_yield(void) {
	bool state = intr_disable();

	spinlock_lock_ni(&curr_thread->lock);
	sched_internal(state);
}

/** Preempt the current thread. */
void sched_preempt(void) {
	bool state = intr_disable();

	curr_cpu->should_preempt = false;

	spinlock_lock_ni(&curr_thread->lock);
	if(curr_thread->preempt_off > 0) {
		curr_thread->preempt_missed = true;
		spinlock_unlock_ni(&curr_thread->lock);
		intr_restore(state);
	} else {
		sched_internal(state);
	}
}

/** Disable preemption.
 *
 * Disables preemption for the current thread. Disables can be nested, so if
 * 2 calls are made to this function, 2 calls to sched_preempt_enable() are
 * required to reenable preemption.
 */
void sched_preempt_disable(void) {
	spinlock_lock(&curr_thread->lock);
	curr_thread->preempt_off++;
	spinlock_unlock(&curr_thread->lock);
}

/** Enable preemption.
 * @note		See sched_preempt_disable() for details of behaviour. */
void sched_preempt_enable(void) {
	bool state = intr_disable();

	spinlock_lock_ni(&curr_thread->lock);

	assert(curr_thread->preempt_off > 0);

	if(--curr_thread->preempt_off == 0) {
		/* If preemption was missed then preempt immediately. */
		if(curr_thread->preempt_missed) {
			curr_thread->preempt_missed = false;
			sched_internal(state);
			return;
		}
	}

	spinlock_unlock_ni(&curr_thread->lock);
	intr_restore(state);
}

/** Scheduler idle thread function.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void sched_idle_thread(void *arg1, void *arg2) {
	intr_disable();
	while(true) {
		sched_yield();
		cpu_idle();
	}
}

/** Initialise the scheduler for the current CPU. */
void __init_text sched_init(void) {
	char name[THREAD_NAME_MAX];
	status_t ret;
	int i, j;

	/* Create the per-CPU information structure. */
	curr_cpu->sched = kmalloc(sizeof(sched_cpu_t), MM_FATAL);
	spinlock_init(&curr_cpu->sched->lock, "sched_lock");
	curr_cpu->sched->total = 0;
	curr_cpu->sched->active = &curr_cpu->sched->queues[0];
	curr_cpu->sched->expired = &curr_cpu->sched->queues[1];

	/* Create the idle thread. */
	sprintf(name, "idle-%" PRIu32, curr_cpu->id);
	ret = thread_create(name, NULL, 0, sched_idle_thread, NULL, NULL, NULL,
	                    &curr_cpu->sched->idle_thread);
	if(ret != STATUS_SUCCESS) {
		fatal("Could not create idle thread for %" PRIu32 " (%d)", curr_cpu->id, ret);
	}
	thread_wire(curr_cpu->sched->idle_thread);

	/* Set the idle thread as the current thread. */
	curr_cpu->sched->idle_thread->cpu = curr_cpu;
	curr_cpu->sched->idle_thread->state = THREAD_RUNNING;
	curr_cpu->sched->prev_thread = NULL;
	curr_cpu->thread = curr_cpu->sched->idle_thread;
	curr_cpu->idle = true;

	/* Create the preemption timer. */
	timer_init(&curr_cpu->sched->timer, sched_timer_handler, NULL, 0);

	/* Initialise queues. */
	for(i = 0; i < 2; i++) {
		curr_cpu->sched->queues[i].bitmap = 0;
		for(j = 0; j < PRIORITY_COUNT; j++) {
			list_init(&curr_cpu->sched->queues[i].threads[j]);
		}
	}
}

/** Begin executing other threads. */
void __init_text sched_enter(void) {
	assert(!intr_state());

	/* Lock the idle thread - sched_post_switch() expects the thread to be
	 * locked. */
	spinlock_lock_ni(&curr_thread->lock);

	/* Restore the idle thread's context. */
	context_restore(&curr_cpu->sched->idle_thread->context);
}
