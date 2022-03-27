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
 * @brief               Mutex implementation.
 */

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <cpu.h>
#include <status.h>

static inline void mutex_recursive_error(mutex_t *lock) {
    #if CONFIG_DEBUG
        fatal(
            "Recursive locking of non-recursive mutex %s (%p)\n"
            "locked at %pB",
            lock->name, lock, lock->caller);
    #else
        fatal("Recursive locking of non-recursive mutex %s (%p)", lock->name, lock);
    #endif
}

static inline status_t mutex_lock_internal(mutex_t *lock, nstime_t timeout, uint32_t flags) {
    status_t ret = STATUS_SUCCESS;

    assert(!in_interrupt());

    unsigned expected = 0;
    if (!atomic_compare_exchange_strong(&lock->value, &expected, 1)) {
        if (lock->holder == curr_thread) {
            if (likely(lock->flags & MUTEX_RECURSIVE)) {
                atomic_fetch_add(&lock->value, 1);
            } else {
                mutex_recursive_error(lock);
            }
        } else {
            spinlock_lock(&lock->lock);

            /* Check again now that we have the lock, in case mutex_unlock() was
             * called on another CPU. */
            expected = 0;
            if (!atomic_compare_exchange_strong(&lock->value, &expected, 1)) {
                list_append(&lock->threads, &curr_thread->wait_link);

                /* If sleep is successful, lock ownership will have been
                 * transferred to us. */
                ret = thread_sleep(&lock->lock, timeout, lock->name, flags);

                /* Still on the list on interrupt or timeout. */
                list_remove(&curr_thread->wait_link);
            }

            spinlock_unlock(&lock->lock);
        }
    }

    if (ret == STATUS_SUCCESS)
        lock->holder = curr_thread;

    return STATUS_SUCCESS;
}

/**
 * Attempts to acquire a mutex. If the mutex has the MUTEX_RECURSIVE flag
 * set, and the calling thread already holds it, the recursion count will be
 * increased. Otherwise, the function will block until the mutex can be
 * acquired, until the timeout expires, or until interrupted (only if
 * SLEEP_INTERRUPTIBLE) is specified.
 *
 * @param lock          Mutex to acquire.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately
 *                      if the lock is currently held by another thread. If -1
 *                      is specified, the thread will sleep indefinitely until
 *                      the lock can be acquired or it is interrupted.
 * @param flags         Sleeping behaviour flags.
 *
 * @return              Status code describing result of the operation. Failure
 *                      is only possible if the timeout is not -1, or if the
 *                      SLEEP_INTERRUPTIBLE flag is set.
 */
status_t mutex_lock_etc(mutex_t *lock, nstime_t timeout, uint32_t flags) {
    status_t ret = mutex_lock_internal(lock, timeout, flags);
    #if CONFIG_DEBUG
        if (likely(ret == STATUS_SUCCESS))
            lock->caller = __builtin_return_address(0);
    #endif

    return ret;
}

/**
 * Acquires a mutex. If the mutex has the MUTEX_RECURSIVE flag set, and the
 * calling thread already holds it, the recursion count will be increased.
 * Otherwise, the function will block until the mutex can be acquired.
 *
 * @param lock          Mutex to acquire.
 */
void mutex_lock(mutex_t *lock) {
    #if CONFIG_DEBUG
        status_t ret = mutex_lock_internal(lock, -1, 0);
        assert(ret == STATUS_SUCCESS);
        lock->caller = __builtin_return_address(0);
    #else
        mutex_lock_internal(lock, -1, 0);
    #endif
}

/**
 * Releases a mutex. Must be held by the current thread, else a fatal error
 * will occur. It is also invalid to release an already unheld mutex. If
 * the mutex has the MUTEX_RECURSIVE flag set, there must be an equal number
 * of calls to this function as there have been to mutex_acquire().
 *
 * @param lock          Mutex to unlock.
 */
void mutex_unlock(mutex_t *lock) {
    spinlock_lock(&lock->lock);

    if (unlikely(!mutex_held(lock))) {
        fatal("Release of unheld mutex %s (%p)", lock->name, lock);
    } else if (unlikely(lock->holder != curr_thread)) {
        fatal(
            "Release of mutex %s (%p) from incorrect thread, expected %" PRIu32,
            lock->name, lock, (lock->holder) ? lock->holder->id : -1);
    }

    /* If the current value is 1, the lock is being released. If there is a
     * thread waiting, we do not need to modify the count, as we transfer
     * ownership of the lock to it. Otherwise, decrement the count. */
    bool woken = false;
    if (atomic_load(&lock->value) == 1) {
        lock->holder = NULL;

        while (!woken && !list_empty(&lock->threads)) {
            thread_t *thread = list_first(&lock->threads, thread_t, wait_link);
            list_remove(&thread->wait_link);
            woken = thread_wake(thread);
        }
    }

    if (!woken)
        atomic_fetch_sub(&lock->value, 1);

    spinlock_unlock(&lock->lock);
}

/** Initializes a mutex.
 * @param lock          Mutex to initialize.
 * @param name          Name to give the mutex.
 * @param flags         Behaviour flags for the mutex. */
void mutex_init(mutex_t *lock, const char *name, uint32_t flags) {
    atomic_store_explicit(&lock->value, 0, memory_order_relaxed);
    spinlock_init(&lock->lock, "mutex_lock");
    list_init(&lock->threads);

    lock->flags  = flags;
    lock->holder = NULL;
    lock->name   = name;
}
