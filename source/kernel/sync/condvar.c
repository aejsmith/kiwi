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
 * @brief               Condition variable implementation.
 */

#include <proc/thread.h>

#include <sync/condvar.h>

#include <assert.h>
#include <cpu.h>

/**
 * Atomically releases a mutex and then blocks until a condition becomes true.
 * The specified mutex should be held by the calling thread. When the function
 * returns (both for success and failure), the mutex will be held again by the
 * calling thread. A condition becomes true when either condvar_signal() or
 * condvar_broadcast() is called on it.
 *
 * @param cv            Condition variable to wait on.
 * @param mutex         Mutex to atomically release while waiting.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately
 *                      if the lock is currently held by another thread. If -1
 *                      is specified, the thread will sleep indefinitely until
 *                      woken or interrupted.
 * @param flags         Sleeping behaviour flags (see proc/thread.h).
 *
 * @return              Status code describing result of the operation. Failure
 *                      is only possible if the timeout is not -1, or if the
 *                      SLEEP_INTERRUPTIBLE flag is set.
 */
status_t condvar_wait_etc(condvar_t *cv, mutex_t *mutex, nstime_t timeout, uint32_t flags) {
    assert(!in_interrupt());

    spinlock_lock(&cv->lock);

    /* Release the specfied lock. */
    if (mutex)
        mutex_unlock(mutex);

    /* Go to sleep. */
    list_append(&cv->threads, &curr_thread->wait_link);
    status_t ret = thread_sleep(&cv->lock, timeout, cv->name, flags);

    /*
     * Still on the list on interrupt or timeout so we need to remove it.
     * thread_sleep() takes cv->lock again.
     *
     * Story here in case I think to try this again in future: I attempted
     * doing this with thread_sleep() not relocking upon return, in an attempt
     * to remove the need to relock if the thread was already removed by a
     * successful wakeup:
     *
     *   if (!list_empty(&curr_thread->wait_link)) {
     *       spinlock_lock(&cv->lock);
     *       list_remove(&curr_thread->wait_link);
     *       spinlock_unlock(&cv->lock);
     *   }
     *
     * This turned out to be unsafe and caused us to hit double (successful)
     * wakeups on mutexes, which I only realised the reason for after a day
     * of debugging. If we're woken by an interrupt/timeout and start running
     * in the window between signal/broadcast removing us from the list and
     * trying to wake us up in thread_wake(), we would not take the lock here
     * because we see that wait_link is not attached. We could then proceed
     * into mutex_lock() and wait on that mutex, in which case the
     * signal/broadcast call that's trying to wake us would then wake us off
     * the mutex rather than the condition variable like it intended.
     *
     * So, we have to take the spinlock here to synchronise with
     * signal/broadcast.
     */
    list_remove(&curr_thread->wait_link);

    spinlock_unlock(&cv->lock);

    /* Re-acquire the lock. */
    if (mutex)
        mutex_lock(mutex);

    return ret;
}

/**
 * Atomically releases a mutex and then blocks until a condition becomes true.
 * The specified mutex should be held by the calling thread. When the function
 * returns the mutex will be held again by the calling thread. A condition
 * becomes true when either condvar_signal() or condvar_broadcast() is called
 * on it.
 *
 * @param cv            Condition variable to wait on.
 * @param mutex         Mutex to atomically release while waiting.
 */
void condvar_wait(condvar_t *cv, mutex_t *mutex) {
    condvar_wait_etc(cv, mutex, -1, 0);
}

/**
 * Wakes the first thread (if any) waiting for a condition variable to become
 * true.
 *
 * @param cv            Condition variable to signal.
 *
 * @return              Whether a thread was woken.
 */
bool condvar_signal(condvar_t *cv) {
    spinlock_lock(&cv->lock);

    bool woken = false;

    while (!woken && !list_empty(&cv->threads)) {
        thread_t *thread = list_first(&cv->threads, thread_t, wait_link);
        list_remove(&thread->wait_link);

        /* If the thread is already woken it means sleep failed but the thread
         * hadn't got a chance to remove itself from the list yet, so we should
         * try to wake another thread. */
        woken = thread_wake(thread);
    }

    spinlock_unlock(&cv->lock);
    return woken;
}

/**
 * Wakes all threads (if any) currently waiting for a condition variable to
 * become true.
 *
 * @param cv            Condition variable to broadcast.
 *
 * @return              Whether any threads were woken.
 */
bool condvar_broadcast(condvar_t *cv) {
    spinlock_lock(&cv->lock);

    bool woken = false;

    while (!list_empty(&cv->threads)) {
        thread_t *thread = list_first(&cv->threads, thread_t, wait_link);
        list_remove(&thread->wait_link);

        /* As with condvar_signal(), return true only for threads that didn't
         * fail sleep. */
        woken |= thread_wake(thread);
    }

    spinlock_unlock(&cv->lock);
    return woken;
}

/** Initializes a condition variable.
 * @param cv            Condition variable to initialize.
 * @param name          Name to give the condition variable. */
void condvar_init(condvar_t *cv, const char *name) {
    spinlock_init(&cv->lock, "condvar_lock");
    list_init(&cv->threads);

    cv->name = name;
}
