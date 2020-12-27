/*
 * Copyright (C) 2009-2020 Alex Smith
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
status_t condvar_wait_etc(condvar_t *cv, mutex_t *mutex, nstime_t timeout, unsigned flags) {
    assert(!in_interrupt());

    spinlock_lock(&cv->lock);

    /* Release the specfied lock. */
    if (mutex)
        mutex_unlock(mutex);

    /* Go to sleep. */
    list_append(&cv->threads, &curr_thread->wait_link);
    status_t ret = thread_sleep(&cv->lock, timeout, cv->name, flags);

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

    bool ret = false;

    if (!list_empty(&cv->threads)) {
        ret = true;

        thread_t *thread = list_first(&cv->threads, thread_t, wait_link);
        thread_wake(thread);
    }

    spinlock_unlock(&cv->lock);
    return ret;
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

    bool ret = false;

    if (!list_empty(&cv->threads)) {
        ret = true;

        while (!list_empty(&cv->threads)) {
            thread_t *thread = list_first(&cv->threads, thread_t, wait_link);
            thread_wake(thread);
        }
    }

    spinlock_unlock(&cv->lock);
    return ret;
}

/** Initializes a condition variable.
 * @param cv            Condition variable to initialize.
 * @param name          Name to give the condition variable. */
void condvar_init(condvar_t *cv, const char *name) {
    spinlock_init(&cv->lock, "condvar_lock");
    list_init(&cv->threads);

    cv->name = name;
}
