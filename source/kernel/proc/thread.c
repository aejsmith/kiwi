/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Thread management code.
 *
 * Below is a brief description of the design for certain parts of the
 * thread management code that require further explanation.
 *
 * Threads contain a reference count to determine when they can safely be
 * freed. When a thread is started running, it is given a reference to indicate
 * that it cannot be freed. Handles to threads and thread pointers in the
 * kernel returned by thread_create() and thread_lookup() add additional
 * references. When a thread exits, the scheduler queues it to its reaper
 * thread, which calls thread_release() on the thread to drop the reference,
 * and if it reaches 0 (i.e. there are no handles/pointers to it left), it will
 * be immediately destroyed.
 *
 * Interruption of sleeping threads is handled using the THREAD_INTERRUPTIBLE
 * and THREAD_INTERRUPTED flags. When a thread goes into interruptible sleep,
 * its THREAD_INTERRUPTIBLE flag is set. When a thread is killed or a user mode
 * interrupt is queued to it, the THREAD_INTERRUPTED flag is set, and if it has
 * THREAD_INTERRUPTIBLE set, the sleep will be interrupted causing the thread's
 * thread_sleep() call to return an error. The THREAD_INTERRUPTED flag is
 * checked by thread_sleep() when entering interruptible sleep. If it is set
 * then the call returns error immediately. This means that if a thread is
 * interrupted while in the kernel but not in interruptible sleep, the interrupt
 * will be handled as soon as the thread reaches a point where it can be
 * interrupted.
 *
 * Threads are killed by setting the THREAD_KILLED flag and interrupting the
 * thread. The THREAD_KILLED flag is checked upon exit from the kernel, and
 * upon entry for a system call. If it is set, the thread will exit.
 *
 * TODO:
 *  - A thread can be prevented from moving CPUs both by disabling preemption
 *    and being wired. The former is the preferred way for a thread to stop
 *    itself being moved. The latter is mainly used at the moment to keep the
 *    idle threads on their CPUs. Once thread affinity is implemented, get rid
 *    of thread_{,un}wire() as that can be used for keeping the idle threads
 *    from moving.
 */

#include <arch/frame.h>
#include <arch/stack.h>

#include <kernel/private/thread.h>

#include <lib/id_allocator.h>
#include <lib/string.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <security/security.h>

#include <sync/mutex.h>
#include <sync/semaphore.h>

#include <assert.h>
#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <smp.h>
#include <status.h>
#include <time.h>

/** Define to enable debug output on thread creation/deletion. */
//#define DEBUG_THREAD

#ifdef DEBUG_THREAD
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Thread creation arguments structure, for thread_user_trampoline(). */
typedef struct thread_user_args {
    ptr_t sp;                       /**< Stack pointer. */
    ptr_t entry;                    /**< Entry point address. */
    ptr_t arg;                      /**< Argument. */
} thread_user_args_t;

static bool thread_timeout(void *_thread);

/** Tree of all threads. */
static AVL_TREE_DEFINE(thread_tree);
static RWLOCK_DEFINE(thread_tree_lock);

/** Thread ID allocator. */
static id_allocator_t thread_id_allocator;

/** Thread structure cache. */
static slab_cache_t *thread_cache;

/** Dead thread queue. */
static LIST_DEFINE(dead_threads);
static SPINLOCK_DEFINE(dead_thread_lock);
static SEMAPHORE_DEFINE(dead_thread_sem, 0);

/** Thread entry function wrapper. */
void thread_trampoline(void) {
    /* Upon switching to a newly-created thread's context, execution will jump
     * to this function, rather than going back to the scheduler. It is
     * therefore necessary to perform post-switch tasks now. */
    sched_post_switch(true);

    dprintf(
        "thread: entered thread %" PRId32 " (%s) on CPU %" PRIu32 "\n",
        curr_thread->id, curr_thread->name, curr_cpu->id);

    /* Set the last time to now so that accounting information is correct. */
    curr_thread->last_time = system_time();

    /* Run the thread's main function and then exit when it returns. */
    curr_thread->func(curr_thread->arg1, curr_thread->arg2);
    thread_exit();
}

static void thread_ctor(void *obj, void *data) {
    thread_t *thread = (thread_t *)obj;

    spinlock_init(&thread->lock, "thread_lock");
    refcount_set(&thread->count, 0);
    list_init(&thread->runq_link);
    list_init(&thread->wait_link);
    list_init(&thread->interrupts);
    list_init(&thread->callbacks);
    list_init(&thread->owner_link);
    timer_init(&thread->sleep_timer, "thread_sleep_timer", thread_timeout, thread, 0);
    notifier_init(&thread->death_notifier, thread);
}

/**
 * Increases the reference count of a thread. This should be done when you
 * want to ensure that the thread will not freed: it will only be freed once
 * the count reaches 0.
 *
 * @param thread        Thread to retain.
 */
void thread_retain(thread_t *thread) {
    refcount_inc(&thread->count);
}

static void thread_cleanup(thread_t *thread) {
    /* If the user stack was allocated by us on behalf of the program, we should
     * unmap it. */
    if (thread->ustack_size)
        vm_unmap(thread->owner->aspace, thread->ustack, thread->ustack_size);

    arch_thread_destroy(thread);
    kmem_free(thread->kstack, KSTACK_SIZE);
    notifier_clear(&thread->death_notifier);

    object_thread_cleanup(thread);

    list_foreach_safe(&thread->interrupts, iter) {
        thread_interrupt_t *interrupt = list_entry(iter, thread_interrupt_t, header);

        list_remove(&interrupt->header);

        if (interrupt->post_cb) {
            interrupt->post_cb(interrupt);
        } else {
            kfree(interrupt);
        }
    }
}

/**
 * Decreases the reference count of a thread. This should be called once you
 * no longer require a thread object (that was returned from thread_create() or
 * thread_lookup(), or that you previously called thread_retain() on). Once the
 * reference count reaches 0, the thread will be destroyed.
 *
 * @param thread        Thread to release.
 */
void thread_release(thread_t *thread) {
    if (refcount_dec(&thread->count) > 0)
        return;

    /* If a thread is running it will have a reference on it. Should not be in
     * the running state for this reason. */
    assert(thread->state == THREAD_CREATED || thread->state == THREAD_DEAD);
    assert(list_empty(&thread->runq_link));

    /* If the thread hasn't been run we still have to clean it up as it will not
     * have gone through thread_exit(). */
    if (thread->state == THREAD_CREATED)
        thread_cleanup(thread);

    rwlock_write_lock(&thread_tree_lock);
    avl_tree_remove(&thread_tree, &thread->tree_link);
    rwlock_unlock(&thread_tree_lock);

    process_detach_thread(thread);
    id_allocator_free(&thread_id_allocator, thread->id);

    dprintf(
        "thread: destroyed thread %" PRId32 " (%s) (thread: %p)\n",
        thread->id, thread->name, thread);

    slab_cache_free(thread_cache, thread);
}

static void reaper_thread(void *arg1, void *arg2) {
    while (true) {
        semaphore_down(&dead_thread_sem);

        /* Take the next thread off the list. */
        spinlock_lock(&dead_thread_lock);

        assert(!list_empty(&dead_threads));
        thread_t *thread = list_first(&dead_threads, thread_t, runq_link);
        list_remove(&thread->runq_link);

        spinlock_unlock(&dead_thread_lock);

        /* Attempt to lock the thread and then unlock it. We must do this to
         * ensure that the scheduler has switched away from the thread, on SMP
         * it's possible that we get here before the thread's CPU has finished
         * with it. */
        spinlock_lock(&thread->lock);
        spinlock_unlock(&thread->lock);

        /* Run death notifications. */
        notifier_run(&thread->death_notifier, NULL, true);
        thread_cleanup(thread);
        process_thread_exited(thread);

        /* Drop the running reference. */
        thread_release(thread);
    }
}

/** Rename a thread.
 * @param thread        Thread to rename.
 * @param name          New name for the thread. */
void thread_rename(thread_t *thread, const char *name) {
    spinlock_lock(&thread->lock);
    strncpy(thread->name, name, THREAD_NAME_MAX);
    thread->name[THREAD_NAME_MAX - 1] = 0;
    spinlock_unlock(&thread->lock);
}

/**
 * Wires a thread to the CPU it is currently running on. If the thread does not
 * have a CPU, it will be set to the current CPU. This prevents it from being
 * moved to another CPU by the scheduler. In order for the thread to be unwired,
 * there must be an equal number of calls to thread_unwire() as there have been
 * to thread_wire().
 *
 * @param thread        Thread to wire.
 */
void thread_wire(thread_t *thread) {
    /* This function may be called during initialization on curr_thread before
     * the thread system is up. Need to handle this case. */
    if (thread) {
        spinlock_lock(&thread->lock);

        thread->wired++;

        /* Wire to the current CPU if there is not a CPU set. */
        if (!thread->cpu)
            thread->cpu = curr_cpu;

        spinlock_unlock(&thread->lock);
    }
}

/**
 * Decreases the wire count of a thread. If the count reaches 0, the thread
 * will be unwired and the scheduler will be allowed to move it to another
 * CPU.
 *
 * @param thread        Thread to unwire.
 */
void thread_unwire(thread_t *thread) {
    if (thread) {
        spinlock_lock(&thread->lock);
        assert(thread->wired > 0);
        thread->wired--;
        spinlock_unlock(&thread->lock);
    }
}

static void thread_wake_unsafe(thread_t *thread) {
    assert(thread->state == THREAD_SLEEPING);
    assert(!thread->wait_lock || spinlock_held(thread->wait_lock));

    /* Stop the timer. */
    timer_stop(&thread->sleep_timer);

    /* Remove the thread from the list and wake it up. */
    list_remove(&thread->wait_link);
    thread->flags &= ~THREAD_INTERRUPTIBLE;
    thread->wait_lock = NULL;

    thread->state = THREAD_READY;
    sched_insert_thread(thread);
}

static inline spinlock_t *acquire_wait_lock(thread_t *thread) {
    spinlock_t *lock;

    /* This is necessary because of kern_futex_requeue(). While we are waiting
     * to acquire the wait lock, it could be changed underneath us. Therefore,
     * we must check whether the lock has changed after acquiring it. */
    while (true) {
        lock = thread->wait_lock;
        if (!lock)
            break;

        spinlock_lock(lock);
        if (likely(thread->wait_lock == lock))
            break;

        spinlock_unlock(lock);
        continue;
    }

    return lock;
}

static bool thread_timeout(void *_thread) {
    thread_t *thread = _thread;

    /* To maintain the correct locking order and prevent deadlock, we must take
     * the wait lock before the thread lock. */
    spinlock_t *lock = acquire_wait_lock(thread);

    spinlock_lock(&thread->lock);

    /* The thread could have been woken up already by another CPU. */
    if (thread->state == THREAD_SLEEPING) {
        thread->sleep_status = STATUS_TIMED_OUT;
        thread_wake_unsafe(thread);
    }

    spinlock_unlock(&thread->lock);

    if (lock)
        spinlock_unlock(lock);

    return false;
}

/**
 * Wakes up a thread that is currently asleep. This function is for use in the
 * implementation of synchronization mechanisms, never call it manually. The
 * lock that thread_sleep() was called with (if any) must be held. The thread
 * will be removed from any wait list it is attached to.
 *
 * @param thread        Thread to wake up.
 */
void thread_wake(thread_t *thread) {
    spinlock_lock(&thread->lock);
    thread_wake_unsafe(thread);
    spinlock_unlock(&thread->lock);
}

static void thread_interrupt_internal(thread_t *thread) {
    /* Set the interrupted flag to signal that there is an unblocked interrupt
     * pending. This flag is checked by thread_sleep() if entering interruptible
     * sleep, if it is set the call will return an error immediately. */
    thread->flags |= THREAD_INTERRUPTED;

    if (thread->state == THREAD_SLEEPING && thread->flags & THREAD_INTERRUPTIBLE) {
        thread->sleep_status = STATUS_INTERRUPTED;
        thread_wake_unsafe(thread);
    } else {
        /* If the thread is running on a different CPU, interrupt it. */
        if (thread->cpu->id != curr_cpu->id && thread->state == THREAD_RUNNING)
            smp_call_single(thread->cpu->id, NULL, NULL, SMP_CALL_ASYNC);
    }
}

/**
 * Asks a userspace thread to terminate as soon as possible (upon next exit
 * from the kernel). If the thread is currently in interruptible sleep, it will
 * be woken. You cannot kill a kernel thread.
 *
 * @param thread        Thread to kill.
 */
void thread_kill(thread_t *thread) {
    if (thread->owner == kernel_proc)
        return;

    /* Correct locking order, see thread_timeout(). */
    spinlock_t *lock = acquire_wait_lock(thread);
    spinlock_lock(&thread->lock);

    thread->flags |= THREAD_KILLED;
    thread_interrupt_internal(thread);

    spinlock_unlock(&thread->lock);
    if (lock)
        spinlock_unlock(lock);
}

/**
 * Queues a user mode interrupt to a thread. Interrupts are queued highest
 * priority first, and in FIFO order within priorities. Once the interrupt
 * reaches the head of the queue and the thread's IPL does not block it, the
 * thread will execute the handler upon its next return to user mode. If the
 * interrupt can be executed immediately and the thread is currently in an
 * interruptible sleep, it will be woken.
 *
 * @param thread        Thread to interrupt.
 * @param interrupt     Interrupt to queue to the thread. If post_cb is set,
 *                      that will be called to handle clean up of the structure,
 *                      otherwise this should be allocated with kmalloc() and
 *                      will be freed after the interrupt has been handled.
 */
void thread_interrupt(thread_t *thread, thread_interrupt_t *interrupt) {
    assert(thread->owner != kernel_proc);
    assert(interrupt->priority <= THREAD_IPL_MAX);
    assert(is_user_address((void *)interrupt->handler));

    list_init(&interrupt->header);

    /* Correct locking order, see thread_timeout(). */
    spinlock_t *lock = acquire_wait_lock(thread);
    spinlock_lock(&thread->lock);

    /* Find where to insert the interrupt. List is ordered by highest priority
     * first, and in FIFO order within priorities. */
    list_foreach(&thread->interrupts, iter) {
        thread_interrupt_t *exist = list_entry(iter, thread_interrupt_t, header);

        if (interrupt->priority > exist->priority) {
            list_add_before(&exist->header, &interrupt->header);
            break;
        }
    }

    /* Place at the end if not already inserted. */
    if (list_empty(&interrupt->header))
        list_append(&thread->interrupts, &interrupt->header);

    /* If the interrupt is the head of the list and the thread's IPL does not
     * block it, wake it up. */
    thread_interrupt_t *first = list_first(&thread->interrupts, thread_interrupt_t, header);
    if (first == interrupt && interrupt->priority >= thread->ipl)
        thread_interrupt_internal(thread);

    spinlock_unlock(&thread->lock);
    if (lock)
        spinlock_unlock(lock);
}

/**
 * Sends the current thread to sleep until it is either woken manually, the
 * given timeout expires, or (if interruptible) it is interrupted. When the
 * function returns, it will no longer be attached to any waiting list in all
 * cases.
 *
 * @param lock          Lock protecting the list on which the thread is waiting,
 *                      if any. Must be locked with IRQ state saved. Will be
 *                      unlocked after the thread has gone to sleep, and will
 *                      not be held when the function returns. Can be NULL.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately.
 *                      If -1 is specified, the thread will sleep indefinitely
 *                      until woken or interrupted.
 * @param name          Name of the object the thread is waiting on, for
 *                      informational purposes.
 * @param flags         Sleeping behaviour flags.
 *
 * @return              STATUS_SUCCESS if woken normally.
 *                      STATUS_TIMED_OUT if timed out.
 *                      STATUS_INTERRUPTED if interrupted.
 *                      STATUS_WOULD_BLOCK if timeout is 0.
 */
status_t thread_sleep(spinlock_t *lock, nstime_t timeout, const char *name, unsigned flags) {
    status_t ret;

    assert(curr_cpu->in_interrupt == ((lock) ? 1 : 0));

    /* If timeout is 0, we return an error immediately. */
    if (timeout == 0) {
        ret = STATUS_WOULD_BLOCK;
        goto cancel;
    }

    /* Convert an absolute target time to a relative time. */
    if (flags & SLEEP_ABSOLUTE && timeout > 0) {
        timeout = timeout - system_time();
        if (timeout <= 0) {
            ret = STATUS_TIMED_OUT;
            goto cancel;
        }
    }

    /* If interruptible and the interrupted flag is set, we also return an error
     * immediately. */
    if (flags & SLEEP_INTERRUPTIBLE && curr_thread->flags & THREAD_INTERRUPTED) {
        ret = STATUS_INTERRUPTED;
        goto cancel;
    }

    /* We're definitely going to sleep. Get the IRQ state to restore. */
    bool irq_state = (lock) ? lock->state : local_irq_disable();

    spinlock_lock_noirq(&curr_thread->lock);
    curr_thread->sleep_status = STATUS_SUCCESS;
    curr_thread->wait_lock = lock;
    curr_thread->waiting_on = name;
    if (flags & SLEEP_INTERRUPTIBLE)
        curr_thread->flags |= THREAD_INTERRUPTIBLE;

    /* Start off the timer if required. */
    if (timeout > 0)
        timer_start(&curr_thread->sleep_timer, timeout, TIMER_ONESHOT);

    /* Drop the specified lock. Do not want to restore IRQ state, we saved it
     * above and it will be restored once we're resumed by the scheduler. */
    if (lock)
        spinlock_unlock_noirq(lock);

    curr_thread->state = THREAD_SLEEPING;
    sched_reschedule(irq_state);
    return curr_thread->sleep_status;

cancel:
    /* The thread must not be attached to the list upon return, nor must the
     * specified lock be held. */
    list_remove(&curr_thread->wait_link);
    if (lock)
        spinlock_unlock(lock);

    return ret;
}

/** Yield remaining timeslice and switch to another thread. */
void thread_yield(void) {
    bool state = local_irq_disable();

    spinlock_lock_noirq(&curr_thread->lock);
    sched_reschedule(state);
}

/** Terminate the current thread (does not return). */
void thread_exit(void) {
    bool irq_state = local_irq_disable();

    spinlock_lock_noirq(&curr_thread->lock);

    /* Queue the thread up to the reaper so that it gets cleaned up after the
     * scheduler has switched away from it. */
    spinlock_lock(&dead_thread_lock);
    list_append(&dead_threads, &curr_thread->runq_link);
    semaphore_up(&dead_thread_sem, 1);
    spinlock_unlock(&dead_thread_lock);

    curr_thread->state = THREAD_DEAD;

    sched_reschedule(irq_state);
    fatal("Shouldn't get here");
}

/** Handle a user mode thread exception.
 * @param info          Exception information structure (will be copied). */
void thread_exception(exception_info_t *info) {
    exception_handler_t handler = curr_thread->exceptions[info->code];
    if (!handler)
        handler = curr_proc->exceptions[info->code];

    if (!handler || curr_thread->ipl > THREAD_IPL_EXCEPTION) {
        kprintf(
            LOG_DEBUG,
            "thread: killing process %" PRId32 " (%s) due to thread %" PRId32 " (%s) exception %u\n",
            curr_proc->id, curr_proc->name, curr_thread->id, curr_thread->name, info->code);

        process_set_exit_status(curr_proc, EXIT_REASON_EXCEPTION, info->code);
        process_exit();
    }

    thread_interrupt_t *interrupt = kcalloc(1, sizeof(*interrupt) + sizeof(*info), MM_KERNEL);

    memcpy(&interrupt->stack, &curr_thread->exception_stack, sizeof(interrupt->stack));

    interrupt->priority = THREAD_IPL_EXCEPTION;
    interrupt->handler  = (ptr_t)handler;
    interrupt->size     = sizeof(*info);

    /* Manually copy member-wise, the structure has padding. */
    exception_info_t *dest_info = (exception_info_t *)(interrupt + 1);
    dest_info->code   = info->code;
    dest_info->addr   = info->addr;
    dest_info->status = info->status;

    thread_interrupt(curr_thread, interrupt);
    assert(curr_thread->flags & THREAD_INTERRUPTED);
}

/** Perform tasks necessary when a thread is entering the kernel. */
void thread_at_kernel_entry(void) {
    /* Update accounting information. */
    nstime_t now = system_time();
    curr_thread->user_time += now - curr_thread->last_time;
    curr_thread->last_time  = now;

    /* Terminate the thread if killed. */
    if (unlikely(curr_thread->flags & THREAD_KILLED)) {
        curr_thread->reason = EXIT_REASON_KILLED;
        thread_exit();
    }
}

/** Perform tasks necessary when a thread is returning to userspace. */
void thread_at_kernel_exit(void) {
    /* Clear active security token. */
    if (unlikely(curr_thread->active_token)) {
        token_release(curr_thread->active_token);
        curr_thread->active_token = NULL;
    }

    /* Check whether we have any pending interrupts to handle. */
    if (unlikely(curr_thread->flags & THREAD_INTERRUPTED)) {
        /* Terminate the thread if killed. */
        if (curr_thread->flags & THREAD_KILLED) {
            curr_thread->reason = EXIT_REASON_KILLED;
            thread_exit();
        }

        spinlock_lock(&curr_thread->lock);

        assert(!list_empty(&curr_thread->interrupts));

        thread_interrupt_t *interrupt = list_first(&curr_thread->interrupts, thread_interrupt_t, header);
        list_remove(&interrupt->header);
        curr_thread->flags &= ~THREAD_INTERRUPTED;

        assert(interrupt->priority >= curr_thread->ipl);

        /* Raise the IPL to block further interrupts. */
        uint32_t ipl = curr_thread->ipl;
        curr_thread->ipl = interrupt->priority + 1;

        spinlock_unlock(&curr_thread->lock);

        status_t ret = arch_thread_interrupt_setup(interrupt, ipl);

        if (interrupt->post_cb) {
            interrupt->post_cb(interrupt);
        } else {
            kfree(interrupt);
        }

        if (unlikely(ret != STATUS_SUCCESS)) {
            /* TODO: Once there is a way to use an alternate stack for an
             * exception handler we should queue up an exception and retry to
             * execute that instead. No point doing so for the moment because
             * trying to do that will just fail again. When implementing this
             * make sure to preserve the original IPL properly. */
            kprintf(
                LOG_WARN, "thread: failed to set up interrupt for thread %" PRId32 " (%" PRId32 "), killing process %" PRId32 "\n",
                curr_thread->id, ret, curr_proc->id);

            process_set_exit_status(curr_proc, EXIT_REASON_KILLED, 0);
            process_exit();
        }
    }

    /* Update accounting information. */
    nstime_t now = system_time();
    curr_thread->kernel_time += now - curr_thread->last_time;
    curr_thread->last_time = now;
}

/**
 * Looks up a thread by its ID, without taking the tree lock. The returned
 * thread will not have an extra reference on it.
 *
 * This function should only be used within KDB. Use thread_lookup() outside of
 * KDB.
 *
 * @param id            ID of the thread to find.
 *
 * @return              Pointer to thread found, or NULL if not found.
 */
thread_t *thread_lookup_unsafe(thread_id_t id) {
    return avl_tree_lookup(&thread_tree, id, thread_t, tree_link);
}

/**
 * Looks up a running thread by its ID. Newly created and dead threads are
 * ignored. If the thread is found, it will be returned with a reference
 * added to it. Once it is no longer needed, thread_release() should be called
 * on it.
 *
 * @param id            ID of the thread to find.
 *
 * @return              Pointer to thread found, or NULL if not found.
 */
thread_t *thread_lookup(thread_id_t id) {
    rwlock_read_lock(&thread_tree_lock);

    thread_t *thread = thread_lookup_unsafe(id);
    if (thread) {
        /* Ignore newly created and dead threads. TODO: Perhaps we want to allow
         * dead threads to be looked up. Possible case: process/thread list
         * utility, we may want to be able to see that there's a dead thread
         * lying around that's being held onto by some handles, and query
         * information. Note that if this is allowed, a change will be necessary
         * to prevent a race condition with thread_release(). */
        if (thread->state == THREAD_DEAD || thread->state == THREAD_CREATED)
            return NULL;

        thread_retain(thread);
    }

    rwlock_unlock(&thread_tree_lock);
    return thread;
}

/**
 * Creates a new thread running in kernel mode. The state the thread will be in
 * depends on whether the _thread argument is non-NULL. If it is NULL, the
 * thread will begin execution immediately. If non-NULL, the thread will be
 * in the Created state, and will have a reference on it. To start execution
 * of the thread, thread_run() must be called on it. When you no longer need
 * access to the thread object, you should call thread_release() to ensure that
 * it will be freed once it has finished running.
 *
 * @param name          Name to give the thread.
 * @param owner         Process that the thread should belong to (if NULL,
 *                      the thread will belong to the kernel process).
 * @param flags         Flags for the thread.
 * @param func          Entry function for the thread.
 * @param arg1          First argument to pass to entry function.
 * @param arg2          Second argument to pass to entry function.
 * @param _thread       Where to store pointer to thread object (can be NULL,
 *                      see above).
 *
 * @return              Status code describing result of the operation.
 */
status_t thread_create(
    const char *name, process_t *owner, unsigned flags, thread_func_t func,
    void *arg1, void *arg2, thread_t **_thread)
{
    assert(name);

    if (!owner)
        owner = kernel_proc;

    thread_t *thread = slab_cache_alloc(thread_cache, MM_KERNEL);

    thread->id = id_allocator_alloc(&thread_id_allocator);
    if (thread->id < 0) {
        slab_cache_free(thread_cache, thread);
        return STATUS_THREAD_LIMIT;
    }

    thread->kstack = kmem_alloc(KSTACK_SIZE, MM_KERNEL);

    arch_thread_init(thread);

    /* Initially set the CPU to NULL - the thread will be assigned to a CPU when
     * thread_run() is called on it. */
    thread->cpu = NULL;

    /* Add a reference if the caller wants a pointer to the thread. */
    if (_thread)
        refcount_inc(&thread->count);

    memset(thread->exceptions, 0, sizeof(thread->exceptions));

    strncpy(thread->name, name, THREAD_NAME_MAX);
    thread->name[THREAD_NAME_MAX - 1] = 0;

    thread->state                = THREAD_CREATED;
    thread->flags                = flags;
    thread->priority             = THREAD_PRIORITY_NORMAL;
    thread->wired                = 0;
    thread->preempt_count        = 0;
    thread->max_prio             = -1;
    thread->curr_prio            = -1;
    thread->timeslice            = 0;
    thread->wait_lock            = NULL;
    thread->last_time            = 0;
    thread->kernel_time          = 0;
    thread->user_time            = 0;
    thread->in_usermem           = false;
    thread->ipl                  = 0;
    thread->exception_stack.base = 0;
    thread->exception_stack.size = 0;
    thread->token                = NULL;
    thread->active_token         = NULL;
    thread->func                 = func;
    thread->arg1                 = arg1;
    thread->arg2                 = arg2;
    thread->ustack               = 0;
    thread->ustack_size          = 0;
    thread->status               = 0;
    thread->reason               = EXIT_REASON_NORMAL;

    /* Add the thread to the owner. */
    process_attach_thread(owner, thread);

    /* Add to the thread tree. */
    rwlock_write_lock(&thread_tree_lock);
    avl_tree_insert(&thread_tree, thread->id, &thread->tree_link);

    dprintf(
        "thread: created thread %" PRId32 " (%s) (thread: %p, owner: %p)\n",
        thread->id, thread->name, thread, owner);

    if (_thread) {
        *_thread = thread;
    } else {
        /* Caller doesn't want a pointer, just start it running. */
        thread_run(thread);
    }

    rwlock_unlock(&thread_tree_lock);
    return STATUS_SUCCESS;
}

/** Run a newly-created thread.
 * @param thread        Thread to run. */
void thread_run(thread_t *thread) {
    process_thread_started(thread);

    spinlock_lock(&thread->lock);

    assert(thread->state == THREAD_CREATED);

    refcount_inc(&thread->count);
    thread->state = THREAD_READY;
    sched_insert_thread(thread);

    spinlock_unlock(&thread->lock);
}

static inline void dump_thread(thread_t *thread) {
    kdb_printf("%-5" PRId32 "%s ", thread->id, (thread == curr_thread) ? "*" : " ");

    switch (thread->state) {
        case THREAD_CREATED:
            kdb_printf("Created      ");
            break;
        case THREAD_READY:
            kdb_printf("Ready        ");
            break;
        case THREAD_RUNNING:
            kdb_printf("Running      ");
            break;
        case THREAD_SLEEPING:
            kdb_printf("Sleeping ");
            if (thread->flags & THREAD_INTERRUPTIBLE) {
                kdb_printf("(I) ");
            } else {
                kdb_printf("    ");
            }
            break;
        case THREAD_DEAD:
            kdb_printf("Dead         ");
            break;
        default:
            kdb_printf("Bad          ");
            break;
    }

    kdb_printf(
        "%-5d %-4" PRIu32 " %-4zu %-4d %-4d 0x%-3x %-24s %-5" PRId32 " %s\n",
        refcount_get(&thread->count), (thread->cpu) ? thread->cpu->id : 0,
        thread->wired, thread->priority, thread->curr_prio, thread->flags,
        (thread->state == THREAD_SLEEPING) ? thread->waiting_on : "<none>",
        thread->owner->id, thread->name);
}

/** Dump a list of threads. */
static kdb_status_t kdb_cmd_thread(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<process ID>]\n\n", argv[0]);

        kdb_printf("Prints a list of all threads, or a list of threads within a process\n");
        kdb_printf("if given a process ID. The ID is given as an expression.\n");
        return KDB_SUCCESS;
    } else if (argc != 1 && argc != 2) {
        kdb_printf("Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    kdb_printf("ID     State        Count CPU  Wire Prio Curr Flags Waiting On               Owner Name\n");
    kdb_printf("==     =====        ===== ===  ==== ==== ==== ===== ==========               ===== ====\n");

    if (argc == 2) {
        /* Find the process ID. */
        uint64_t pid;
        if (kdb_parse_expression(argv[1], &pid, NULL) != KDB_SUCCESS)
            return KDB_FAILURE;

        process_t *process = process_lookup_unsafe(pid);
        if (!process) {
            kdb_printf("Invalid process ID.\n");
            return KDB_FAILURE;
        }

        list_foreach(&process->threads, iter) {
            thread_t *thread = list_entry(iter, thread_t, owner_link);
            dump_thread(thread);
        }
    } else {
        avl_tree_foreach(&thread_tree, iter) {
            thread_t *thread = avl_tree_entry(iter, thread_t, tree_link);
            dump_thread(thread);
        }
    }

    return KDB_SUCCESS;
}

/** Kill a thread. */
static kdb_status_t kdb_cmd_kill(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s [<thread ID>]\n\n", argv[0]);

        kdb_printf("Schedules a currently running thread to be killed once KDB exits.\n");
        kdb_printf("Note that this has no effect on kernel threads.\n");
        return KDB_SUCCESS;
    } else if (argc != 2) {
        kdb_printf("Incorrect number of argments. See 'help %s' for help.\n", argv[0]);
        return KDB_FAILURE;
    }

    uint64_t tid;
    if (kdb_parse_expression(argv[1], &tid, NULL) != KDB_SUCCESS)
        return KDB_FAILURE;

    thread_t *thread = thread_lookup_unsafe(tid);
    if (!thread) {
        kdb_printf("Invalid thread ID.\n");
        return KDB_FAILURE;
    }

    thread_kill(thread);
    return KDB_SUCCESS;
}

/** Initialize the thread system. */
__init_text void thread_init(void) {
    /* Initialize the thread ID allocator. */
    id_allocator_init(&thread_id_allocator, 65535, MM_BOOT);

    /* Create the thread slab cache. */
    thread_cache = object_cache_create(
        "thread_cache",
        thread_t, thread_ctor, NULL, NULL, 0, MM_BOOT);

    /* Register our KDB commands. */
    kdb_register_command("thread", "Print information about threads.", kdb_cmd_thread);
    kdb_register_command("kill", "Kill a running user thread.", kdb_cmd_kill);

    /* Initialize the scheduler. */
    sched_init();

    /* Create the thread reaper. */
    status_t ret = thread_create("reaper", NULL, 0, reaper_thread, NULL, NULL, NULL);
    if (ret != STATUS_SUCCESS)
        fatal("Could not create thread reaper (%d)", ret);
}

/**
 * System calls.
 */

/** Closes a handle to a thread. */
static void thread_object_close(object_handle_t *handle) {
    thread_release(handle->private);
}

/** Signal that a thread is being waited for. */
static status_t thread_object_wait(object_handle_t *handle, object_event_t *event) {
    thread_t *thread = handle->private;

    switch (event->event) {
        case THREAD_EVENT_DEATH:
            if (thread->state == THREAD_DEAD) {
                /* For edge-triggered, there's no point adding to the notifier if
                * it's already dead, it won't become dead again. */
                if (!(event->flags & OBJECT_EVENT_EDGE))
                    object_event_signal(event, 0);
            } else {
                notifier_register(&thread->death_notifier, object_event_notifier, event);
            }

            return STATUS_SUCCESS;
        default:
            return STATUS_INVALID_EVENT;
    }
}

/** Stop waiting for a thread. */
static void thread_object_unwait(object_handle_t *handle, object_event_t *event) {
    thread_t *thread = handle->private;

    switch (event->event) {
        case THREAD_EVENT_DEATH:
            notifier_unregister(&thread->death_notifier, object_event_notifier, event);
            break;
    }
}

/** Thread object type. */
static const object_type_t thread_object_type = {
    .id     = OBJECT_TYPE_THREAD,
    .flags  = OBJECT_TRANSFERRABLE,
    .close  = thread_object_close,
    .wait   = thread_object_wait,
    .unwait = thread_object_unwait,
};

static status_t thread_handle_lookup(handle_t handle, thread_t **_thread) {
    if (handle == THREAD_SELF) {
        refcount_inc(&curr_thread->count);

        *_thread = curr_thread;
        return STATUS_SUCCESS;
    }

    object_handle_t *khandle;
    status_t ret = object_handle_lookup(handle, OBJECT_TYPE_THREAD, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    thread_t *thread = khandle->private;
    refcount_inc(&thread->count);
    object_handle_release(khandle);

    *_thread = thread;
    return STATUS_SUCCESS;
}

static void thread_user_trampoline(void *_args, void *arg2) {
    thread_user_args_t *args = _args;
    frame_t frame;

    arch_thread_user_setup(&frame, args->entry, args->sp, args->arg);
    kfree(args);
    arch_thread_user_enter(&frame);
}

/**
 * Creates a new thread within the calling process and start it executing at
 * the given entry function. If a stack is provided for the thread, that will
 * be used, and the stack will not be freed when the thread exits. Otherwise,
 * a stack will be allocated for the thread with a default size, and will be
 * freed when the thread exits.
 *
 * @param name          Name to give the thread.
 * @param entry         Thread entry point.
 * @param arg           Argument to pass to the entry point.
 * @param stack         Details of the stack to use/allocate for the new thread
 *                      (optional). See the documentation for thread_stack_t
 *                      for details.
 * @param flags         Creation behaviour flags.
 * @param _handle       Where to store handle to the thread (optional).
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_thread_create(
    const char *name, thread_entry_t entry, void *arg,
    const thread_stack_t *stack, uint32_t flags, handle_t *_handle)
{
    status_t ret;

    if (!name || !entry)
        return STATUS_INVALID_ARG;

    if (!is_user_address(entry))
        return STATUS_INVALID_ADDR;

    thread_stack_t kstack;
    if (stack) {
        ret = memcpy_from_user(&kstack, stack, sizeof(kstack));
        if (ret != STATUS_SUCCESS)
            return ret;

        if (kstack.base) {
            if (!kstack.size) {
                return STATUS_INVALID_ARG;
            } else if (!is_user_range(kstack.base, kstack.size)) {
                return STATUS_INVALID_ADDR;
            }
        }
    } else {
        kstack.base = NULL;
        kstack.size = 0;
    }

    char *kname;
    ret = strndup_from_user(name, THREAD_NAME_MAX, &kname);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Create arguments structure. This will be freed by the entry trampoline. */
    thread_user_args_t *args = kmalloc(sizeof(thread_user_args_t), MM_KERNEL);
    args->entry = (ptr_t)entry;
    args->arg   = (ptr_t)arg;

    /* Create the thread, but do not run it yet. We attempt to create the handle
     * to the thread before running it as this allows us to terminate it if not
     * successful. */
    thread_t *thread;
    ret = thread_create(kname, curr_proc, 0, thread_user_trampoline, args, NULL, &thread);
    if (ret != STATUS_SUCCESS)
        goto err_free_args;

    /* Create a handle to the thread if necessary. */
    handle_t uhandle = -1;
    if (_handle) {
        refcount_inc(&thread->count);

        object_handle_t *khandle = object_handle_create(&thread_object_type, thread);
        ret = object_handle_attach(khandle, &uhandle, _handle);
        object_handle_release(khandle);

        if (ret != STATUS_SUCCESS)
            goto err_free_thread;
    }

    /* Create a userspace stack. TODO: Stack direction! */
    if (kstack.base) {
        args->sp = (ptr_t)kstack.base + kstack.size;
    } else {
        if (kstack.size) {
            kstack.size = round_up(kstack.size, PAGE_SIZE);
        } else {
            kstack.size = USTACK_SIZE;
        }

        char stack_name[THREAD_NAME_MAX];
        snprintf(stack_name, sizeof(stack_name), "%s_stack", kname);

        ret = vm_map(
            curr_proc->aspace, &thread->ustack, kstack.size, kstack.size,
            VM_ADDRESS_ANY, VM_ACCESS_READ | VM_ACCESS_WRITE,
            VM_MAP_PRIVATE | VM_MAP_STACK, NULL, 0, stack_name);
        if (ret != STATUS_SUCCESS)
            goto err_free_thread;

        /* Stack will be unmapped when the thread exits by thread_cleanup(). Do
         * not set until here as will be freed if ustack_size is non-zero, so if
         * we set above and then fail vm_map(), thread_cleanup() will try to
         * free. */
        thread->ustack_size = kstack.size;

        args->sp = thread->ustack + kstack.size;
    }

    thread_run(thread);
    thread_release(thread);

    kfree(kname);
    return STATUS_SUCCESS;

err_free_thread:
    if (uhandle >= 0)
        object_handle_detach(uhandle);

    if (thread)
        thread_release(thread);

err_free_args:
    kfree(args);
    kfree(kname);
    return ret;
}

/** Opens a handle to a thread.
 * @param id            ID of the thread to open, or THREAD_SELF for calling
 *                      thread.
 * @param _handle       Where to store handle to thread.
 * @return              Status code describing result of the operation. */
status_t kern_thread_open(thread_id_t id, handle_t *_handle) {
    if (!_handle)
        return STATUS_INVALID_ARG;

    thread_t *thread;
    if (id == THREAD_SELF) {
        thread = curr_thread;
        thread_retain(thread);
    } else {
        thread = thread_lookup(id);
        if (!thread)
            return STATUS_NOT_FOUND;
    }

    /* Reference added by thread_lookup() is taken over by this handle. */
    status_t ret = object_handle_open(&thread_object_type, thread, NULL, _handle);
    if (ret != STATUS_SUCCESS)
        thread_release(thread);

    return ret;
}

/** Gets the ID of a thread.
 * @param handle        Handle to thread, or THREAD_SELF for calling thread.
 * @param _id           Where to store ID of thread.
 * @return              Status code describing result of the operation. */
status_t kern_thread_id(handle_t handle, thread_id_t *_id) {
    thread_t *thread;
    status_t ret = thread_handle_lookup(handle, &thread);
    if (ret == STATUS_SUCCESS) {
        ret = write_user(_id, thread->id);
        thread_release(thread);
    }

    return ret;
}

/**
 * Gets the given thread's current security context. If the thread has an
 * overridden security token, the context contained in that will be returned,
 * otherwise the context from the process-wide token will be returned. The
 * calling thread must have privileged access to the process owning the thread,
 * i.e. the user IDs must match, or it must have the PRIV_PROCESS_ADMIN
 * privilege. This is only useful to query a thread's current identity, as it
 * returns only the context content, rather than a token object containing it.
 *
 * @param handle        Handle to thread, or THREAD_SELF for calling thread.
 * @param ctx           Where to store context of the thread.
 *
 * @return              Status code describing result of the operation.
 */
status_t kern_thread_security(handle_t handle, security_context_t *ctx) {
    status_t ret;

    if (!ctx)
        return STATUS_INVALID_ARG;

    thread_t *thread;
    ret = thread_handle_lookup(handle, &thread);
    if (ret != STATUS_SUCCESS)
        return ret;

    mutex_lock(&thread->owner->lock);

    if (!process_access_unsafe(thread->owner)) {
        mutex_unlock(&thread->owner->lock);
        thread_release(thread);
        return STATUS_ACCESS_DENIED;
    }

    token_t *token = (thread->token) ? thread->token : thread->owner->token;
    token_retain(token);

    mutex_unlock(&thread->owner->lock);

    ret = memcpy_to_user(ctx, &token->ctx, sizeof(token->ctx));

    token_release(token);
    thread_release(thread);
    return ret;
}

/** Query the exit status of a thread.
 * @param handle        Handle to thread.
 * @param _status       Where to store exit status of thread (can be NULL).
 * @param _reason       Where to store exit reason (can be NULL).
 * @return              Status code describing result of the operation. */
status_t kern_thread_status(handle_t handle, int *_status, int *_reason) {
    status_t ret;

    /* Although getting the status of the current thread is silly (it'll error
     * below), support it anyway for consistency's sake. */
    thread_t *thread;
    ret = thread_handle_lookup(handle, &thread);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (!process_access(thread->owner)) {
        ret = STATUS_ACCESS_DENIED;
    } else if (thread->state != THREAD_DEAD) {
        ret = STATUS_STILL_RUNNING;
    }

    if (ret == STATUS_SUCCESS && _status)
        ret = write_user(_status, thread->status);

    if (ret == STATUS_SUCCESS && _reason)
        ret = write_user(_reason, thread->reason);

    thread_release(thread);
    return ret;
}

/**
 * Kills the thread (i.e. cause it to immediately exit) referred to by the
 * specified handle. The calling thread must have privileged access to the
 * process owning the thread.
 *
 * @param handle        Handle to thread.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle is invalid.
 *                      STATUS_ACCESS_DENIED if the caller does not have
 *                      privileged access to the thread.
 */
status_t kern_thread_kill(handle_t handle) {
    status_t ret;

    thread_t *thread;
    ret = thread_handle_lookup(handle, &thread);
    if (ret != STATUS_SUCCESS)
        return ret;

    if (!process_access(thread->owner)) {
        ret = STATUS_ACCESS_DENIED;
    } else {
        thread_kill(thread);
    }

    thread_release(thread);
    return ret;
}

/**
 * Gets the calling thread's current interrupt priority level (IPL). The IPL
 * is used to block interrupts to a thread's normal flow of execution. There
 * are 2 sources of thread interrupts: exceptions and asynchronous object event
 * notifications. Exceptions are raised at a fixed priority, and object event
 * notifications are raised at the priority they are registered with. Any
 * interrupts raised with a priority lower than the thread's current IPL will
 * not be handled until the IPL is set less than or equal to that priority.
 * When an interrupt handler is executed, the IPL is raised to 1 higher than
 * the interrupt's priority, and the previous IPL is restored once the handler
 * returns.
 *
 * @param _ipl          Where to store current IPL.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if _ipl is NULL.
 *                      STATUS_INVALID_ADDR if _ipl is an invalid address.
 */
status_t kern_thread_ipl(uint32_t *_ipl) {
    if (!_ipl)
        return STATUS_INVALID_ARG;

    return write_user(_ipl, curr_thread->ipl);
}

/**
 * Sets the calling thread's current interrupt priority level (IPL) (see
 * kern_thread_ipl() for a description of the IPL). If the new IPL is lower
 * than the current IPL, any pending interrupts which have become unblocked
 * will be executed.
 *
 * @param mode          How to set the IPL (THREAD_SET_IPL_*).
 * @param ipl           New IPL (must be less than or equal to THREAD_IPL_MAX).
 * @param _prev_ipl     Where to store previous IPL (can be NULL).
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if the new IPL or the mode is invalid.
 */
status_t kern_thread_set_ipl(uint32_t mode, uint32_t ipl, uint32_t *_prev_ipl) {
    if (ipl > THREAD_IPL_MAX)
        return STATUS_INVALID_ARG;

    status_t ret = STATUS_SUCCESS;

    spinlock_lock(&curr_thread->lock);

    uint32_t prev_ipl = curr_thread->ipl;

    bool should_set = false;
    switch (mode) {
        case THREAD_SET_IPL_ALWAYS:
            should_set = true;
            break;

        case THREAD_SET_IPL_RAISE:
            should_set = ipl > prev_ipl;
            break;

        default:
            ret = STATUS_INVALID_ARG;
            break;
    }

    if (should_set) {
        curr_thread->ipl = ipl;

        /* Check whether there are any pending interrupts that have now become
         * unblocked. If so, set the flag so that they will be executed when we
         * return to user mode. */
        curr_thread->flags &= ~THREAD_INTERRUPTED;
        if (!list_empty(&curr_thread->interrupts)) {
            thread_interrupt_t *interrupt = list_first(&curr_thread->interrupts, thread_interrupt_t, header);
            if (interrupt->priority >= ipl)
                curr_thread->flags |= THREAD_INTERRUPTED;
        }
    }

    spinlock_unlock(&curr_thread->lock);

    if (ret == STATUS_SUCCESS && _prev_ipl)
        ret = write_user(_prev_ipl, prev_ipl);

    return ret;
}

/**
 * Gets a handle to the calling thread's current overridden security token.
 * If the thread does not have an overridden token, the function will return
 * STATUS_SUCCESS, but the location referred to by _handle will be set to
 * INVALID_HANDLE.
 *
 * @param _handle       Where to store handle to token.
 *
 * @return              Status code describing the result of the operation.
 */
status_t kern_thread_token(handle_t *_handle) {
    status_t ret;

    if (!_handle)
        return STATUS_INVALID_ARG;

    mutex_lock(&curr_proc->lock);

    if (curr_thread->token) {
        ret = token_publish(curr_thread->token, NULL, _handle);
    } else {
        ret = write_user(_handle, INVALID_HANDLE);
    }

    mutex_unlock(&curr_proc->lock);

    return ret;
}

/**
 * Sets the overridden security token of the calling thread to the given token
 * object. The overridden security token can be used by a thread to temporarily
 * change its security context to that contained in another token and take on
 * the privileges it grants. When set, it is used instead of the process-wide
 * security token for security checks. If called with INVALID_HANDLE, the thread
 * will revert to the process-wide security token.
 *
 * @param handle        Handle to token, INVALID_HANDLE to revert to process-
 *                      wide security token.
 *
 * @return              Status code describing the result of the operation.
 */
status_t kern_thread_set_token(handle_t handle) {
    status_t ret;

    token_t *token = NULL;

    if (handle != INVALID_HANDLE) {
        object_handle_t *khandle;
        ret = object_handle_lookup(handle, OBJECT_TYPE_TOKEN, &khandle);
        if (ret != STATUS_SUCCESS)
            return ret;

        token = khandle->private;
        token_retain(token);
        object_handle_release(khandle);
    }

    mutex_lock(&curr_proc->lock);

    if (curr_thread->token)
        token_release(curr_thread->token);

    curr_thread->token = token;

    mutex_unlock(&curr_proc->lock);
    return STATUS_SUCCESS;
}

/**
 * Sets an exception handler for the current thread. Each thread has its own
 * set of exception handlers in addition to the process-wide handlers. If a
 * handler is set to non-NULL in a thread, it will be used instead of the
 * process-wide handler (if any). If there is neither a per-thread handler or a
 * process-wide handler for an exception that occurs, the whole process is
 * killed.
 *
 * @param code          Exception to set handler for.
 * @param handler       Handler function to use (NULL to unset the per-thread
 *                      handler).
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if code is invalid.
 *                      STATUS_INVALID_ADDR if handler is an invalid address.
 */
status_t kern_thread_set_exception_handler(unsigned code, exception_handler_t handler) {
    if (code >= EXCEPTION_MAX) {
        return STATUS_INVALID_ARG;
    } else if (handler && !is_user_address(handler)) {
        return STATUS_INVALID_ADDR;
    }

    curr_thread->exceptions[code] = handler;
    return STATUS_SUCCESS;
}

/**
 * Sets an alternate stack to use when handling exceptions in the calling
 * thread. The exception stack is set per-thread. The specified stack will be
 * switched to whenever executing an exception handler on this thread.
 *
 * @param stack         Description of new stack to use. A specific stack range
 *                      must be specified - this function will not allocate a
 *                      stack. If NULL, the alternate exception stack will be
 *                      disabled and exceptions will be handled on the main
 *                      stack.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if stack base is NULL or size is 0.
 *                      STATUS_INVALID_ADDR if stack range is invalid.
 */
status_t kern_thread_set_exception_stack(const thread_stack_t *stack) {
    if (stack) {
        thread_stack_t kstack;
        status_t ret = memcpy_from_user(&kstack, stack, sizeof(kstack));
        if (ret != STATUS_SUCCESS)
            return ret;

        if (!kstack.base || !kstack.size) {
            return STATUS_INVALID_ARG;
        } else if (!is_user_range(kstack.base, kstack.size)) {
            return STATUS_INVALID_ADDR;
        }

        memcpy(&curr_thread->exception_stack, &kstack, sizeof(kstack));
    } else {
        curr_thread->exception_stack.base = 0;
        curr_thread->exception_stack.size = 0;
    }

    return STATUS_SUCCESS;
}

/**
 * Raises an exception in the calling thread. If a handler is registered and
 * the current IPL does not block exceptions, then the handler will be executed.
 * Otherwise, the process will be terminated.
 *
 * @param info          Exception information.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_ARG if info is NULL or exception code is
 *                      invalid.
 *                      STATUS_INVALID_ADDR if info points to an invalid address.
 */
status_t kern_thread_raise(exception_info_t *info) {
    if (!info)
        return STATUS_INVALID_ARG;

    exception_info_t kinfo;
    status_t ret = memcpy_from_user(&kinfo, info, sizeof(kinfo));
    if (ret != STATUS_SUCCESS)
        return ret;

    if (kinfo.code >= EXCEPTION_MAX)
        return STATUS_INVALID_ARG;

    thread_exception(&kinfo);
    return STATUS_SUCCESS;
}

/** Sleeps for a certain amount of time.
 * @param nsecs         Number of nanoseconds to sleep for. Must be 0 or
 *                      higher.
 * @param _rem          If not NULL, the number of nanoseconds remaining will
 *                      be stored here if the wait is interrupted.
 * @return              Status code describing result of the operation. */
status_t kern_thread_sleep(nstime_t nsecs, nstime_t *_rem) {
    status_t ret;

    if (nsecs < 0)
        return STATUS_INVALID_ARG;

    // TODO: Once system call restarting is implemented, don't return
    // remaining time. To be accurate we'd calculate target time and use
    // SLEEP_ABSOLUTE, and repeatedly wait until we reach that target.

    /* FIXME: The method getting remaining time isn't quite accurate. */
    nstime_t begin = system_time();

    ret = thread_sleep(NULL, nsecs, "kern_thread_sleep", SLEEP_INTERRUPTIBLE);
    if (likely(ret == STATUS_TIMED_OUT || ret == STATUS_WOULD_BLOCK))
        ret = STATUS_SUCCESS;

    if (ret == STATUS_INTERRUPTED && _rem) {
        nstime_t elapsed = system_time() - begin;
        if (elapsed < nsecs) {
            write_user(_rem, nsecs - elapsed);
        } else {
            ret = STATUS_SUCCESS;
        }
    }

    return ret;
}

/** Terminates the calling thread.
 * @param status        Exit status code. */
void kern_thread_exit(int status) {
    curr_thread->status = status;
    thread_exit();
}

/** Performs operations on the current thread (for internal use by libkernel).
 * @param action        Action to perform.
 * @param in            Pointer to input buffer.
 * @param out           Pointer to output buffer.
 * @return              Status code describing result of the operation. */
status_t kern_thread_control(unsigned action, const void *in, void *out) {
    switch (action) {
        case THREAD_SET_TLS_ADDR:
            if (!is_user_address(in))
                return STATUS_INVALID_ADDR;

            arch_thread_set_tls_addr((ptr_t)in);
            return STATUS_SUCCESS;
        default:
            return STATUS_INVALID_ARG;
    }
}

/** Restores previous state upon return from an interrupt handler. */
void kern_thread_restore(void) {
    status_t ret;

    uint32_t ipl;
    ret = arch_thread_interrupt_restore(&ipl);
    if (ret != STATUS_SUCCESS) {
        /* TODO: Same as in thread_at_kernel_exit(). */
        process_set_exit_status(curr_proc, EXIT_REASON_KILLED, 0);
        process_exit();
    }

    ret = kern_thread_set_ipl(THREAD_SET_IPL_ALWAYS, ipl, NULL);
    if (ret != STATUS_SUCCESS) {
        process_set_exit_status(curr_proc, EXIT_REASON_KILLED, 0);
        process_exit();
    }

    /* The architecture code should have set things up such that we return into
     * the pre-interrupt state. */
}
