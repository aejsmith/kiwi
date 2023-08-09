/*
 * Copyright (C) 2009-2023 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               Thread scheduler.
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
 * TODO:
 *  - Load balancing could be improved in situations where there are several
 *    CPU-bound threads running. Currently, a thread is only moved to another
 *    CPU when it is woken from sleep. This could be done by stealing threads
 *    from other CPUs when a CPU becomes idle.
 *  - Once the facility to get CPU topology information is implemented, we
 *    should be more friendly to HT systems when picking a CPU for a thread to
 *    run on by favouring idle logical CPUs on a core that is not already
 *    running a thread on another of its logical CPUs.
 *  - Possibly a better heuristic for determining whether a thread is CPU-/IO-
 *    bound is to look at how much time it spends sleeping.
 *  - Alter timeslice based on priority?
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <smp.h>
#include <status.h>
#include <time.h>

/** Define to enable extremely verbose debug output. */
//#define DEBUG_SCHED

#ifdef DEBUG_SCHED
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Number of priority levels. */
#define PRIORITY_COUNT      32

/** Timeslice to give to threads. */
#define THREAD_TIMESLICE    msecs_to_nsecs(3)

/** Maximum penalty to CPU-bound threads. */
#define MAX_PENALTY         5

/** Run queue structure. */
typedef struct sched_queue {
    unsigned long bitmap;               /**< Bitmap of queues with data. */
    list_t threads[PRIORITY_COUNT];     /**< Queues of runnable threads. */
} sched_queue_t;

/** Per-CPU scheduling information structure. */
typedef struct sched_cpu {
    spinlock_t lock;                    /**< Lock to protect information/queues. */

    thread_t *prev_thread;              /**< Previously executed thread. */

    thread_t *idle_thread;              /**< Thread scheduled when no other threads runnable. */
    timer_t timer;                      /**< Preemption timer. */
    sched_queue_t *active;              /**< Active queue. */
    sched_queue_t *expired;             /**< Expired queue. */
    sched_queue_t queues[2];            /**< Active and expired queues. */
    size_t total;                       /**< Total running/ready thread count. */
} sched_cpu_t;

/** Total number of running/ready threads across all CPUs. */
static atomic_uint threads_running;

static inline void sched_queue_insert(sched_queue_t *queue, thread_t *thread) {
    list_append(&queue->threads[thread->curr_prio], &thread->runq_link);
    queue->bitmap |= (1 << thread->curr_prio);
}

static inline void sched_queue_remove(sched_queue_t *queue, thread_t *thread) {
    list_remove(&thread->runq_link);
    if (list_empty(&queue->threads[thread->curr_prio]))
        queue->bitmap &= ~(1 << thread->curr_prio);
}

static inline void sched_calculate_priority(thread_t *thread) {
    /* We need to map the priority class and the thread priority onto our 32
     * priority levels. See documentation/sched-priorities.txt for a pretty bad
     * explanation of what this is doing. */
    int priority = 5 + (thread->owner->priority * 8);
    priority += (thread->priority - 1) * 2;

    dprintf(
        "sched: setting priority for thread %" PRId32 " to %d (proc: %d, thread: %d)\n",
        thread->id, priority, thread->owner->priority, thread->priority);

    thread->max_prio = thread->curr_prio = priority;
}

static inline void sched_tweak_priority(sched_cpu_t *cpu, thread_t *thread) {
    if (thread->timeslice != 0) {
        /* The timeslice wasn't fully used, give a bonus if we're not already
         * at the maximum priority. */
        if (thread->curr_prio < thread->max_prio) {
            thread->curr_prio++;

            dprintf(
                "sched: thread %" PRId32 " (%" PRId32 ") bonus (new: %d, max: %d)\n",
                thread->id, thread->owner->id, thread->curr_prio, thread->max_prio);
        }
    } else {
        /* The timeslice was used up, penalise the thread if we've not already
         * given it the maximum penalty. */
        if (thread->curr_prio && (thread->max_prio - thread->curr_prio) < MAX_PENALTY) {
            thread->curr_prio--;

            dprintf(
                "sched: thread %" PRId32 " (%" PRId32 ") penalty (new: %d, max: %d)\n",
                thread->id, thread->owner->id, thread->curr_prio, thread->max_prio);
        }
    }
}

static thread_t *sched_pick_thread(sched_cpu_t *cpu) {
    if (!cpu->active->bitmap) {
        /* The active queue is empty. If there are threads on the expired queue,
         * swap them. */
        if (cpu->expired->bitmap) {
            swap(cpu->active, cpu->expired);
        } else {
            return NULL;
        }
    }

    /* Get the thread to run. */
    int i = fls(cpu->active->bitmap) - 1;
    thread_t *thread = list_first(&cpu->active->threads[i], thread_t, runq_link);
    sched_queue_remove(cpu->active, thread);
    return thread;
}

/** Scheduler timer handler function.
 * @param data          Data argument (unused).
 * @return              Always returns true. */
static bool sched_timer_handler(void *data) {
    curr_thread->timeslice = 0;
    return true;
}

/**
 * Main function of the scheduler. Picks a new thread to run and switches to
 * it. Interrupts must be disabled (explicitly, before taking the thread lock),
 * and the current thread must be locked. When the thread is next run and this
 * function returns, the thread lock will have been released and the given
 * interrupt state will have been restored.
 *
 * Never call this function. Use one of the thread methods to trigger a
 * reschedule (e.g. thread_yield()).
 *
 * @param state         Previous interrupt state.
 */
void sched_reschedule(bool state) {
    sched_cpu_t *cpu = curr_cpu->sched;

    spinlock_lock_noirq(&cpu->lock);

    /* Stop the preemption timer if it is running. */
    timer_stop(&cpu->timer);

    /* Thread can't be in ready state if we're running it now. */
    assert(curr_thread->state != THREAD_READY);

    /* Tweak the priority of the thread based on whether it used up its
     * timeslice. */
    if (curr_thread != cpu->idle_thread)
        sched_tweak_priority(cpu, curr_thread);

    if (curr_thread->state == THREAD_RUNNING) {
        /* The thread hasn't gone to sleep, re-queue it. */
        curr_thread->state = THREAD_READY;

        if (curr_thread != cpu->idle_thread)
            sched_queue_insert(cpu->expired, curr_thread);
    } else {
        /* Thread is no longer running, decrease counts. */
        assert(curr_thread != cpu->idle_thread);
        cpu->total--;

        atomic_fetch_sub(&threads_running, 1);
    }

    /* Find a new thread to run. A NULL return value means no threads are ready,
     * so we schedule the idle thread in this case. */
    thread_t *next = sched_pick_thread(cpu);
    if (next) {
        if (next != curr_thread)
            spinlock_lock_noirq(&next->lock);

        next->timeslice = THREAD_TIMESLICE;
        curr_cpu->idle = false;
    } else {
        next = cpu->idle_thread;
        if (next != curr_thread) {
            spinlock_lock_noirq(&next->lock);

            dprintf(
                "sched: cpu %" PRIu32 " has no runnable threads remaining, idling\n",
                curr_cpu->id);
        }

        /* The idle thread runs indefinitely until an interrupt causes something
         * to be woken up. */
        next->timeslice = 0;
        curr_cpu->idle = true;
    }

    assert(next->cpu == curr_cpu);
    assert(next->state == THREAD_READY);

    /* Move the thread to the running state. */
    cpu->prev_thread = curr_thread;
    next->state = THREAD_RUNNING;
    curr_cpu->thread = next;

    /* Finished with the scheduler queues, unlock. */
    spinlock_unlock_noirq(&cpu->lock);

    /* Set off the timer if necessary. */
    if (next->timeslice > 0)
        timer_start(&cpu->timer, next->timeslice, TIMER_ONESHOT);

    /* Only need to continue if the new thread is different. */
    if (next != cpu->prev_thread) {
        /* Switch the address space. This function handles the case where the
         * new process' aspace pointer is NULL. */
        vm_aspace_switch(next->owner->aspace);

        /* Perform the thread switch. This should change over the curr_thread
         * pointer to the new thread. */
        arch_thread_switch(next, cpu->prev_thread);

        /* The switch may return to thread_trampoline(), so put anything to do
         * after a switch in sched_post_switch() below. */
        sched_post_switch(state);
    } else {
        spinlock_unlock_noirq(&next->lock);
        local_irq_restore(state);
    }
}

/** Perform post-thread-switch tasks.
 * @param state         Interrupt state to restore. */
__noinline void sched_post_switch(bool state) {
    /* This function must not be inlined to prevent the compiler from making
     * incorrect optimisations across a thread switch (e.g. assuming that the
     * value of curr_thread has not changed). */
    spinlock_unlock_noirq(&curr_thread->lock);

    /* The prev_thread pointer is set to NULL during sched_init(). It will only
     * ever be NULL once. */
    if (likely(curr_cpu->sched->prev_thread)) {
        /* Unlock previous. This will not be the current thread, as a switch is
         * not performed if the new thread is the same as the previous. */
        spinlock_unlock_noirq(&curr_cpu->sched->prev_thread->lock);
    }

    local_irq_restore(state);
}

/** Preempt the current thread. */
void sched_preempt(void) {
    bool state = local_irq_disable();

    curr_cpu->should_preempt = false;

    spinlock_lock_noirq(&curr_thread->lock);

    if (curr_thread->preempt_count > 0) {
        thread_set_flag(curr_thread, THREAD_PREEMPTED);
        spinlock_unlock_noirq(&curr_thread->lock);
        local_irq_restore(state);
    } else {
        sched_reschedule(state);
    }
}

/**
 * Prevents the current thread from being preempted. This also prevents the
 * thread from being moved to a different CPU. This function can be nested,
 * i.e. in order for preemption to be enabled again, there must be an equal
 * number of calls to preempt_enable() as there have been to preempt_disable().
 */
void preempt_disable(void) {
    if (curr_thread)
        curr_thread->preempt_count++;
}

/**
 * Decrements the preemption disable count. If it reaches 0, preemption will
 * be re-enabled for the current thread. If a preemption was missed due to
 * being disabled, the thread will be preempted immediately unless interrupts
 * are currently disabled.
 */
void preempt_enable(void) {
    if (!curr_thread)
        return;

    bool irq_state = local_irq_disable();

    assert(curr_thread->preempt_count > 0);

    if (--curr_thread->preempt_count == 0 && irq_state) {
        spinlock_lock_noirq(&curr_thread->lock);

        /* If a preemption was missed then preempt immediately. */
        if (thread_flags(curr_thread) & THREAD_PREEMPTED) {
            thread_clear_flag(curr_thread, THREAD_PREEMPTED);
            sched_reschedule(irq_state);
            return;
        }

        spinlock_unlock_noirq(&curr_thread->lock);
    }

    local_irq_restore(irq_state);
}

static inline cpu_t *sched_allocate_cpu(thread_t *thread) {
    /* On uniprocessor systems, we only have one choice. */
    if (cpu_count == 1)
        return curr_cpu;

    /* Add 1 to the total number of threads to account for the thread we're
     * adding. */
    size_t total = atomic_load(&threads_running) + 1;

    /* Start on the current CPU that the thread currently belongs to. */
    cpu_t *cpu = (thread->cpu) ? thread->cpu : curr_cpu;

    /* Get the average number of threads that a CPU should have as well as the
     * CPU's current load. We round up to a multiple of the CPU count rather
     * than rounding down here to stop us from loading off threads
     * unnecessarily. */
    size_t average = (round_up(total, cpu_count) / cpu_count);
    size_t load    = cpu->sched->total;

    /* If the CPU has less than or equal to the average, we're OK to keep the
     * thread on it. */
    if (load <= average)
        return cpu;

    /* We need to pick another CPU. Try to find one with a load less than the
     * average. If there isn't one, we just keep the thread on its current CPU. */
    list_foreach(&running_cpus, iter) {
        cpu_t *other = list_entry(iter, cpu_t, header);

        load = other->sched->total;
        if (load < average) {
            dprintf(
                "sched: CPU %" PRIu32 " load %zu less than average %zu, giving it thread %" PRId32 "\n",
                other->id, load, average, thread->id);

            cpu = other;
            break;
        }
    }

    return cpu;
}

/** Insert a thread into the scheduler.
 * @param thread        Thread to insert (should be locked and in the ready
 *                      state). */
void sched_insert_thread(thread_t *thread) {
    assert(thread->state == THREAD_READY);

    /* If we've been newly created, we will not have a priority set. Calculate
     * the maximum priority for the thread. */
    if (unlikely(thread->max_prio < 0))
        sched_calculate_priority(thread);

    /* Pick a new CPU for the thread to run on. */
    if (!thread->wired && !thread->preempt_count)
        thread->cpu = sched_allocate_cpu(thread);

    sched_cpu_t *sched = thread->cpu->sched;
    spinlock_lock(&sched->lock);

    sched_queue_insert(sched->active, thread);
    sched->total++;

    atomic_fetch_add(&threads_running, 1);

    /* If the thread has a higher priority than the currently running thread on
     * the CPU, or if the CPU is idle, preempt it. */
    if (thread->cpu->idle || thread->curr_prio > thread->cpu->thread->curr_prio) {
        if (!thread->cpu->idle)
            thread->cpu->should_preempt = true;

        if (thread->cpu != curr_cpu)
            smp_call_single(thread->cpu->id, NULL, NULL, SMP_CALL_ASYNC);
    }

    spinlock_unlock(&sched->lock);
}

static void sched_idle_thread(void *arg1, void *arg2) {
    /* We run the loop with interrupts disabled. The arch_cpu_idle() function is
     * expected to re-enable interrupts as required. */
    local_irq_disable();

    while (true) {
        spinlock_lock_noirq(&curr_thread->lock);
        sched_reschedule(false);

        arch_cpu_idle();
    }
}

/** Initialize the scheduler globally. */
__init_text void sched_init(void) {
    /* Initialize for the boot CPU. */
    sched_init_percpu();
}

/** Initialize the scheduler for the current CPU. */
__init_text void sched_init_percpu(void) {
    /* Create the per-CPU information structure. */
    curr_cpu->sched = kmalloc(sizeof(sched_cpu_t), MM_BOOT);
    spinlock_init(&curr_cpu->sched->lock, "sched_lock");
    curr_cpu->sched->total = 0;
    curr_cpu->sched->active = &curr_cpu->sched->queues[0];
    curr_cpu->sched->expired = &curr_cpu->sched->queues[1];

    /* Create the idle thread. */
    char name[THREAD_NAME_MAX];
    sprintf(name, "idle-%" PRIu32, curr_cpu->id);
    status_t ret = thread_create(
        name, NULL, 0, sched_idle_thread, NULL, NULL,
        &curr_cpu->sched->idle_thread);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create idle thread for %" PRIu32 " (%d)", curr_cpu->id, ret);

    thread_wire(curr_cpu->sched->idle_thread);

    /* Set the idle thread as the current thread. */
    refcount_inc(&curr_cpu->sched->idle_thread->count);
    curr_cpu->sched->idle_thread->cpu = curr_cpu;
    curr_cpu->sched->idle_thread->state = THREAD_RUNNING;
    curr_cpu->sched->prev_thread = NULL;
    curr_cpu->thread = curr_cpu->sched->idle_thread;
    curr_cpu->idle = true;

    /* Create the preemption timer. */
    timer_init(&curr_cpu->sched->timer, "sched_timer", sched_timer_handler, NULL, 0);

    /* Initialize queues. */
    for (int i = 0; i < 2; i++) {
        curr_cpu->sched->queues[i].bitmap = 0;
        for (int j = 0; j < PRIORITY_COUNT; j++)
            list_init(&curr_cpu->sched->queues[i].threads[j]);
    }
}

/** Begin executing other threads. */
__init_text void sched_enter(void) {
    /* Lock the idle thread - sched_post_switch() expects it to be locked. */
    spinlock_lock_noirq(&curr_cpu->thread->lock);

    /* Switch to the idle thread. */
    arch_thread_switch(curr_cpu->thread, NULL);
    fatal("Should not get here");
}
