/* Kiwi thread scheduler
 * Copyright (C) 2008-2009 Alex Smith
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
 * The thread scheduler maintains per-CPU prioritized run queues. The highest
 * priority is 0, then 1, then 2, etc, until PRIORITY_MAX is reached. Each
 * CPU has an array of linked lists, one for each priority. When picking a
 * thread to run, the scheduler goes through the CPU's run queues, starting
 * at the highest priority, until a thread is found. This means that higher
 * priority threads will always be scheduled before lower priority threads.
 *
 * However, this can introduce starvation problems for lower priority threads.
 * To prevent this, when switching threads the scheduler checks whether the
 * previous thread used all of its timeslice. If it didn't, its priority is
 * increased by 1, unless it is at its owner's maximum. Otherwise, the
 * scheduler checks if it is preventing any other threads running, and if it
 * is its priority is decreased by 1.
 *
 * Threads are also assigned a timeslice based on their priority. The current
 * timeslice algorithm is (thread priority + 1) milliseconds.
 *
 * Because the highest priority is 0, this means that higher priority processes
 * will get run more frequently than lower priority processes, but will run
 * for shorter periods.
 *
 * On SMP systems, load balancing is performed by a set of threads, one for
 * each CPU. A count of all runnable threads across all CPUs is manintained,
 * which is used by the load balancer thread to work out the average number
 * of threads that a CPU should have. If a CPU has less threads than this
 * average, then its load balancer pulls threads from overloaded CPUs.
 */

#include <arch/sched.h>
#include <arch/stack.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/kheap.h>
#include <mm/malloc.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>

#if CONFIG_SCHED_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Set to 1 to enable a debug message for every thread switch. */
#define SCHED_OVERKILL_DEBUG	0

/** Per-CPU scheduling information structure. */
typedef struct sched_cpu {
	spinlock_t lock;		/**< Lock to protect information/queues. */

	thread_t *prev_thread;		/**< Previously executed thread. */

	thread_t *idle_thread;		/**< Thread scheduled when no other threads runnable. */
	thread_t *balancer_thread;	/**< Load balancing thread. */
	timer_t timer;			/**< Preemption timer. */

	list_t queues[PRIORITY_MAX];	/**< Prioritized runnable thread queues. */
	size_t count[PRIORITY_MAX];	/**< Count of threads in run queues. */
	atomic_t runnable;		/**< Total count of runnable threads. */
} sched_cpu_t;

extern void sched_internal(bool state);
extern void sched_post_switch(bool state);
extern void sched_thread_insert(thread_t *thread);

/** Total runnable threads across all CPUs. */
static atomic_t threads_runnable = 0;

/** Migrate a thread from another CPU to current CPU.
 * @param cpu		Source CPU's scheduler structure (should be locked).
 * @param thread	Thread to migrate.
 * @return		1 if successful, 0 if not. */
static inline int sched_migrate_thread(sched_cpu_t *cpu, thread_t *thread) {
	spinlock_lock_ni(&thread->lock, 0);

	assert(thread->cpu->sched == cpu);
	assert(thread->state == THREAD_READY);

	/* Don't move unmovable threads. */
	if(thread->flags & THREAD_UNMOVABLE) {
		spinlock_unlock_ni(&thread->lock);
		return 0;
	}

	dprintf("sched: migrating thread %" PRIu32 "(%s) to CPU %" PRIu32 " from CPU %" PRIu32 "\n",
		thread->id, thread->name, curr_cpu->id, thread->cpu->id);

	/* Remove the thread from its old CPU. */
	list_remove(&thread->header);
	cpu->count[thread->priority]--;
	atomic_dec(&cpu->runnable);

	thread->cpu = curr_cpu;

	/* Drop the source CPU lock temporarily while we work on the current
	 * CPU to prevent deadlock. Interrupts are managed by the caller so we
	 * do not need to worry about the state. */
	spinlock_unlock_ni(&cpu->lock);

	/* Insert it in the current CPU's queue. */
	spinlock_lock_ni(&curr_cpu->sched->lock, 0);
	curr_cpu->sched->count[thread->priority]++;
	list_append(&curr_cpu->sched->queues[thread->priority], &thread->header);
	atomic_inc(&curr_cpu->sched->runnable);
	spinlock_unlock_ni(&curr_cpu->sched->lock);
	spinlock_unlock_ni(&thread->lock);

	/* Retake the source CPU lock. */
	spinlock_lock_ni(&cpu->lock, 0);
	return 1;
}

/** Migrate threads from another CPU.
 * @param cpu		Source CPU's scheduler structure (should be locked).
 * @param priority	Priority queue to migrate from.
 * @param max		Maximum number of threads to migrate.
 * @return		Number of threads migrated. */
static inline int sched_migrate_cpu(sched_cpu_t *cpu, int priority, int max) {
	thread_t *thread;
	int count = max;

	LIST_FOREACH_SAFE(&cpu->queues[priority], iter) {
		thread = list_entry(iter, thread_t, header);

		if((count -= sched_migrate_thread(cpu, thread)) == 0) {
			break;
		}
	}

	return (max - count);
}

/** Attempt to migrate threads with a certain priority.
 * @param average	Average number of threads per CPU.
 * @param priority	Thread priority to migrate.
 * @param max		Maximum number of threads to migrate.
 * @return		Number of threads migrated. */
static inline int sched_migrate_priority(int average, int priority, int max) {
	bool state = intr_disable();
	int load, num, count = max;
	cpu_t *cpu;

	LIST_FOREACH(&cpus_running, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu == curr_cpu) {
			continue;
		}

		spinlock_lock_ni(&cpu->sched->lock, 0);

		/* Check whether the CPU has some threads that we can take. */
		load = atomic_get(&cpu->sched->runnable);
		if(load <= average) {
			dprintf("sched: cpu %" PRIu32 " with load %d average %d has no threads for %" PRIu32 "\n",
				cpu->id, load, average, curr_cpu->id);
			spinlock_unlock_ni(&cpu->sched->lock);
			continue;
		}

		/* Calculate how many threads to take from this CPU. */
		num = (load - average) < count ? (load - average) : count;
		dprintf("sched: migrating at most %d from priority %d on %" PRIu32 " (count: %d, max: %d)\n",
			num, priority, cpu->id, count, max);

		/* Take as many threads as we can. */
		count -= sched_migrate_cpu(cpu->sched, priority, num);
		spinlock_unlock_ni(&cpu->sched->lock);

		/* If count is now zero, then we have nothing left to do. */
		assert(count >= 0);
		if(count == 0) {
			intr_restore(state);
			return max;
		}
	}

	intr_restore(state);
	return (max - count);
}

/** Per-CPU load balancing thread.
 * @param arg1		First thread argument.
 * @param arg2		Second thread argument. */
static void sched_balancer_thread(void *arg1, void *arg2) {
	int total, load, average, count, i;

	while(true) {
		/* Sleep for 3 seconds. */
		timer_sleep(3);

		dprintf("sched: load-balancer for CPU %" PRIu32 " woken\n", curr_cpu->id);

		/* Check if there are any threads available. */
		total = atomic_get(&threads_runnable);
		if(total == 0) {
			dprintf("sched: total thread count is 0, nothing to do\n");
			continue;
		}

		/* Get the average number of threads that a CPU should have as
		 * well as our current load. We round up to a multiple of the
		 * CPU count rather than rounding down here for a good reason.
		 * As an example, we have an 8 CPU box, and there are 15
		 * runnable threads. If we round down, then the average will
		 * be 1. This could result in all but one CPU having 1 thread,
		 * and one CPU having 8 threads (the other CPUs won't pull
		 * threads off this CPU to themselves if they have the average
		 * of one, and CPUs don't give threads away either). Rounding
		 * up the total count will ensure that this doesn't happen. */
		average = (ROUND_UP(total, cpu_count) / cpu_count);
		load = atomic_get(&curr_cpu->sched->runnable);

		/* If this CPU has the average or more than the average we
		 * don't need to do anything. It is up to other CPUs to take
		 * threads from this CPU. */
		if(load >= average) {
			dprintf("sched: load %d greater than or equal to average %d, nothing to do\n", load, average);
			continue;
		}

		/* There are not enough threads on this CPU, work out how many
		 * we need and find some to take from other CPUs. Low priority
		 * threads are migrated before higher priority threads. */
		count = average - load;
		for(i = (PRIORITY_MAX - 1); i >= 0; i--) {
			count -= sched_migrate_priority(average, i, count);
			if(count == 0) {
				break;
			}
		}

		continue;
	}
}

/** Tweak priority of a thread being stored.
 * @param cpu		Per-CPU scheduler information structure.
 * @param thread	Thread to tweak. */
static inline void sched_tweak_priority(sched_cpu_t *cpu, thread_t *thread) {
	size_t i;

	/* If the timeslice wasn't fully used, give a bonus if we're not
	 * already at the process' maximum. */
	if(thread->timeslice != 0) {
		if(thread->priority > thread->owner->priority) {
			thread->priority--;
			dprintf("sched: thread %" PRIu32 " (" PRIu32 ") bonus (new: %" PRIs ", max: %" PRIs ")\n",
				thread->id, thread->owner->id, thread->priority, thread->owner->priority);
		}

		return;
	}

	/* Check if there are any higher or equal priority threads. */
	for(i = 0; i <= thread->priority; i++) {
		/* If there are, then this thread is not preventing other
		 * things from running so no penalties are required. */
		if(cpu->count[i] > 0) {
			return;
		}
	}

	/* Check if there are any lower priority threads. The conditions for
	 * this loop guarantee that no penalty will be given if the thread
	 * is already at the lowest priority - it simply won't run. */
	for(i = thread->priority + 1; i < PRIORITY_MAX; i++) {
		/* If there are, then this thread is preventing others from
		 * running, so we give it a priority penalty of +1. */
		if(cpu->count[i] > 0) {
			thread->priority++;

			dprintf("sched: thread %" PRIu32 " (" PRIu32 ") penalty (new: %" PRIs ", max: %" PRIs ")\n",
				thread->id, thread->owner->id, thread->priority, thread->owner->priority);
			return;
		}		
	}
}

/** Pick a new thread to run.
 * @param cpu		Per-CPU scheduler information structure.
 * @return		Pointer to thread, or NULL if no threads available. */
static thread_t *sched_queue_pick(sched_cpu_t *cpu) {
	thread_t *thread;
	int i;

	/* Loop through each queue, starting at the highest priority, to find
	 * a thread to run. */
	for(i = 0; i < PRIORITY_MAX; i++) {
		if(cpu->count[i] == 0) {
			continue;
		}

		/* Pick the first thread off the queue. */
		thread = list_entry(cpu->queues[i].next, thread_t, header);
		list_remove(&thread->header);
		cpu->count[i]--;

		atomic_dec(&threads_runnable);
		atomic_dec(&curr_cpu->sched->runnable);

		/* Only lock the new thread if it isn't the current - the
		 * current gets locked by sched_internal(). */
		if(thread != curr_thread) {
			spinlock_lock_ni(&thread->lock, 0);
		}

		/* Calculate a new timeslice for the thread using the algorithm
		 * described at the top of the file. */
		thread->timeslice = ((thread->priority + 1) * 1000000);

		return thread;
	}

	return NULL;
}

/** Tweak thread priority and store in run queue.
 * @param cpu		Per-CPU scheduler information structure.
 * @param thread	Thread to store. */
static void sched_queue_store(sched_cpu_t *cpu, thread_t *thread) {
	/* Tweak priority of the thread if required. */
	if((thread->owner->flags & PROCESS_FIXEDPRIO) == 0) {
		sched_tweak_priority(cpu, thread);
	}

	assert(thread->priority < PRIORITY_MAX);

	cpu->count[thread->priority]++;
	list_append(&cpu->queues[thread->priority], &thread->header);

	atomic_inc(&curr_cpu->sched->runnable);
	atomic_inc(&threads_runnable);
}

/*
 * Generic scheduler functions.
 */

/** Scheduler timer handler function.
 * @return		Whether to perform a thread switch. */
static bool sched_timer_handler(void) {
	bool ret = true;

	spinlock_lock(&curr_thread->lock, 0);

	curr_thread->timeslice = 0;
	if(curr_thread->preempt_off > 0) {
		curr_thread->preempt_missed = true;
		ret = false;
	}

	spinlock_unlock(&curr_thread->lock);
	return ret;
}

/** Internal part of the thread scheduler. Expects current thread to be locked.
 * @param state		Previous interrupt state. */
void sched_internal(bool state) {
	sched_cpu_t *cpu = curr_cpu->sched;
	thread_t *new;

	spinlock_lock_ni(&cpu->lock, 0);

	/* Thread can't be in ready state if we're running it now. */
	assert(curr_thread->state != THREAD_READY);

	/* If this thread hasn't gone to sleep then dump it on the end of
	 * the run queue */
	if(curr_thread->state == THREAD_RUNNING) {
		curr_thread->state = THREAD_READY;
		if(!(curr_thread->flags & THREAD_UNQUEUEABLE)) {
			sched_queue_store(cpu, curr_thread);
		}
	}

	/* Find a new thread to run. A NULL return value means no threads are
	 * ready, so we schedule the idle thread in this case. This will
	 * return with the new thread locked if it is not the current. */
	new = sched_queue_pick(cpu);
	if(new == NULL) {
		new = cpu->idle_thread;
		if(new != curr_thread) {
			spinlock_lock_ni(&new->lock, 0);
			dprintf("sched: cpu %" PRIu32 " has no runnable threads remaining, idling\n", curr_cpu->id);
		}
		new->timeslice = 0;

		/* Mark the current CPU as idle. */
		curr_cpu->idle = true;
	} else {
		curr_cpu->idle = false;
	}

	/* Move the thread to the Running state and set it as the current. */
	cpu->prev_thread = curr_thread;
	new->state = THREAD_RUNNING;
	curr_thread = new;

	/* Finished with the scheduler queues, unlock. */
	spinlock_unlock_ni(&cpu->lock);

#if SCHED_OVERKILL_DEBUG
	kprintf(LOG_DEBUG, "sched: switching to thread %" PRIu32 "(%s) (process: %" PRIu32 ", cpu: %" PRIu32 ")\n",
		curr_thread->id, curr_thread->name, curr_thread->owner->id, curr_cpu->id);
#endif

	/* Set off the timer if necessary. */
	if(!(curr_thread->flags & THREAD_UNPREEMPTABLE)) {
		assert(curr_thread->timeslice > 0);
		if(timer_start(&cpu->timer, curr_thread->timeslice) != 0) {
			fatal("Could not set scheduler timer for %" PRIu32, curr_cpu->id);
		}
	}

	/* Only bother with this stuff if the new thread is different.
	 * The switch may return to thread_trampoline() or to the interruption
	 * handler in wait_queue_sleep(), so put anything to do after a switch
	 * in sched_post_switch(). */
	if(curr_thread != cpu->prev_thread) {
		/* Switch the address space. If the new process' addres space
		 * is set to NULL then aspace_switch() will just switch to
		 * the kernel address space. */
		aspace_switch(curr_thread->owner->aspace);

		if(context_save(&cpu->prev_thread->context) == 0) {
			context_restore(&curr_thread->context);
		}
	}

	sched_post_switch(state);
}

/** Perform post-thread-switch tasks.
 * @param state		Interrupt state to restore. */
void sched_post_switch(bool state) {
	/* Set the current CPU pointer. */
	cpu_set_pointer((ptr_t)cpus[cpu_current_id()]);

	/* Do architecture-specific post-switch tasks. */
	sched_arch_post_switch();

	spinlock_unlock_ni(&curr_thread->lock);
	if(curr_thread != curr_cpu->sched->prev_thread) {
		spinlock_unlock_ni(&curr_cpu->sched->prev_thread->lock);

		/* Deal with thread terminations. */
		if(curr_cpu->sched->prev_thread->state == THREAD_DEAD) {
			thread_destroy(curr_cpu->sched->prev_thread);
		}
	}

	intr_restore(state);
}

/** Insert a thread into its CPU's run queue.
 * @param thread	Thread to insert (should be locked). */
void sched_thread_insert(thread_t *thread) {
	assert(thread->state == THREAD_READY);
	assert(!(thread->flags & THREAD_UNQUEUEABLE));

	spinlock_lock(&thread->cpu->sched->lock, 0);
	sched_queue_store(thread->cpu->sched, thread);
	spinlock_unlock(&thread->cpu->sched->lock);

	if(thread->cpu != curr_cpu && thread->cpu->idle) {
		cpu_reschedule(thread->cpu);
	}
}

/** Switch to another thread.
 *
 * Yields the remainder of the calling thread's timeslice and switches to
 * another thread.
 */
void sched_yield(void) {
	bool state = intr_disable();

	spinlock_lock_ni(&curr_thread->lock, 0);
	sched_internal(state);
}

/** Disable preemption.
 *
 * Disables preemption for the current thread. Disables can be nested, so if
 * 2 calls are made to this function, 2 calls to sched_preempt_enable() are
 * required to reenable preemption.
 */
void sched_preempt_disable(void) {
	spinlock_lock(&curr_thread->lock, 0);
	curr_thread->preempt_off++;
	spinlock_unlock(&curr_thread->lock);
}

/** Enable preemption.
 *
 * Enables preemption for the current thread. See documentation for
 * sched_preempt_disable().
 */
void sched_preempt_enable(void) {
	spinlock_lock(&curr_thread->lock, 0);

	if(curr_thread->preempt_off <= 0) {
		fatal("Preemption already enabled or negative");
	} else if(--curr_thread->preempt_off == 0) {
		/* If preemption was missed then preempt immediately. */
		if(curr_thread->preempt_missed) {
			curr_thread->preempt_missed = false;
			spinlock_unlock(&curr_thread->lock);
			sched_yield();
		} else {
			spinlock_unlock(&curr_thread->lock);
		}
	}
}

/** Scheduler idle loop. */
void sched_idle(void) {
	/* Interrupts should be disabled here. */
	assert(intr_state() == false);

	while(true) {
		sched_yield();
		sched_cpu_idle();
	}
}

/** Initialize the scheduler for the current CPU. */
void sched_init(void) {
	char name[THREAD_NAME_MAX];
	int i;

	/* Create the per-CPU information structure. */
	curr_cpu->sched = kmalloc(sizeof(sched_cpu_t), MM_FATAL);
	spinlock_init(&curr_cpu->sched->lock, "sched_lock");
	atomic_set(&curr_cpu->sched->runnable, 0);

	/* Create the idle thread. */
	sprintf(name, "idle-%" PRIu32, curr_cpu->id);
	if(thread_create(name, kernel_proc, THREAD_UNMOVABLE | THREAD_UNQUEUEABLE | THREAD_UNPREEMPTABLE,
	                 NULL, NULL, NULL, &curr_cpu->sched->idle_thread) != 0) {
		fatal("Could not create idle thread for %" PRIu32, curr_cpu->id);
	}

	/* The boot code becomes the idle thread, so free the stack that was
	 * allocated for it and point it at the current stack. We also set the
	 * current state to running and point the current thread to it. */
	kheap_free(curr_cpu->sched->idle_thread->kstack, KSTACK_SIZE);
	curr_cpu->sched->idle_thread->kstack = stack_get_base();
	curr_cpu->sched->idle_thread->cpu = curr_cpu;
	curr_cpu->sched->idle_thread->state = THREAD_RUNNING;
	curr_cpu->thread = curr_cpu->sched->idle_thread;
	curr_cpu->idle = true;

	/* Create the preemption timer. */
	timer_init(&curr_cpu->sched->timer, TIMER_FUNCTION, sched_timer_handler);

	/* Initialize run queues. */
	for(i = 0; i < PRIORITY_MAX; i++) {
		list_init(&curr_cpu->sched->queues[i]);
		curr_cpu->sched->count[i] = 0;
	}

	/* Create the load-balancing thread if we have more than one CPU. */
	if(cpu_count > 1) {
		sprintf(name, "balancer-%" PRIu32, curr_cpu->id);
		if(thread_create(name, kernel_proc, THREAD_UNMOVABLE | THREAD_UNPREEMPTABLE,
		                 sched_balancer_thread, NULL, NULL, &curr_cpu->sched->balancer_thread) != 0) {
			fatal("Could not create load balancer thread for %" PRIu32, curr_cpu->id);
		}
		thread_run(curr_cpu->sched->balancer_thread);
	}
}
