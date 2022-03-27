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
 * @brief               Readers-writer lock implementation.
 *
 * Ideas for this implementation, particularly on how to prevent thread
 * starvation, are from HelenOS' readers-writer lock implementation.
 */

#include <proc/thread.h>

#include <sync/rwlock.h>

#include <assert.h>
#include <cpu.h>
#include <status.h>

typedef struct rwlock_waiter {
    list_t link;
    thread_t *thread;
    bool is_writer;
} rwlock_waiter_t;

/**
 * Transfers lock ownership to a waiting writer or waiting readers. Queue lock
 * should be held.
 */
static void rwlock_transfer_ownership(rwlock_t *lock) {
    /* Check if there are any threads to transfer ownership to. */
    if (list_empty(&lock->waiters)) {
        /* There aren't. If there are still readers (it is possible for there to
         * be, because this function gets called if a writer is interrupted
         * while blocking in order to allow readers queued behind it in), then
         * we do not want to do anything. Otherwise, release the lock. */
        if (!lock->readers)
            lock->held = 0;
    } else {
        /* Go through all threads queued. */
        list_foreach_safe(&lock->waiters, iter) {
            rwlock_waiter_t *waiter = list_entry(iter, rwlock_waiter_t, link);

            /* If it is a reader, we can wake it and continue. If it is a writer
             * and the lock has no readers, wake it up and finish. If it is a
             * writer and the lock has readers, finish. */
            if (waiter->is_writer && lock->readers) {
                break;
            } else {
                list_remove(&waiter->link);

                if (thread_wake(waiter->thread)) {
                    if (waiter->is_writer) {
                        break;
                    } else {
                        lock->readers++;
                    }
                }
            }
        }
    }
}

/**
 * Acquires a readers-writer lock for reading. Multiple readers can hold a
 * readers-writer lock at any one time, however if there are any writers
 * waiting for the lock, the function will block and allow the writer to take
 * the lock, in order to prevent starvation of writers.
 *
 * @param lock          Lock to acquire.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately
 *                      if the lock cannot be acquired immediately. If -1
 *                      is specified, the thread will sleep indefinitely until
 *                      the lock can be acquired or it is interrupted.
 * @param flags         Sleeping behaviour flags.
 *
 * @return              Status code describing result of the operation. Failure
 *                      is only possible if the timeout is not -1, or if the
 *                      SLEEP_INTERRUPTIBLE flag is set.
 */
status_t rwlock_read_lock_etc(rwlock_t *lock, nstime_t timeout, uint32_t flags) {
    assert(!in_interrupt());

    spinlock_lock(&lock->lock);

    if (lock->held) {
        /* Lock is held, check if it's held by readers. If it is, and there's
         * something waiting on the queue, we wait anyway. This is to prevent
         * starvation of writers. */
        if (!lock->readers || !list_empty(&lock->waiters)) {
            rwlock_waiter_t waiter;
            waiter.thread    = curr_thread;
            waiter.is_writer = false;

            /* Readers count will have been incremented for us upon success. */
            list_init(&waiter.link);
            list_append(&lock->waiters, &waiter.link);
            status_t ret = thread_sleep(&lock->lock, timeout, lock->name, flags);

            /* Still on the list on interrupt or timeout. */
            list_remove(&waiter.link);

            spinlock_unlock(&lock->lock);
            return ret;
        }
    } else {
        lock->held = 1;
    }

    lock->readers++;

    spinlock_unlock(&lock->lock);
    return STATUS_SUCCESS;
}

/**
 * Acquires a readers-writer lock for writing. When the lock has been acquired,
 * no other readers or writers will be holding the lock, or be able to acquire
 * it.
 *
 * @param lock          Lock to acquire.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately
 *                      if the lock cannot be acquired immediately. If -1
 *                      is specified, the thread will sleep indefinitely until
 *                      the lock can be acquired or it is interrupted.
 * @param flags         Sleeping behaviour flags.
 *
 * @return              Status code describing result of the operation. Failure
 *                      is only possible if the timeout is not -1, or if the
 *                      SLEEP_INTERRUPTIBLE flag is set.
 */
status_t rwlock_write_lock_etc(rwlock_t *lock, nstime_t timeout, uint32_t flags) {
    assert(!in_interrupt());

    status_t ret = STATUS_SUCCESS;

    spinlock_lock(&lock->lock);

    /* Just acquire the exclusive lock. */
    if (lock->held) {
        rwlock_waiter_t waiter;
        waiter.thread    = curr_thread;
        waiter.is_writer = true;

        list_init(&waiter.link);
        list_append(&lock->waiters, &waiter.link);
        ret = thread_sleep(&lock->lock, timeout, lock->name, flags);

        /* Still on the list on interrupt or timeout. */
        list_remove(&waiter.link);

        /* If we failed to acquire the lock, there may be a reader queued
         * behind us that can be let in. */
        if (ret != STATUS_SUCCESS && lock->readers)
            rwlock_transfer_ownership(lock);
    } else {
        lock->held = 1;
    }

    spinlock_unlock(&lock->lock);
    return ret;
}

/**
 * Acquires a readers-writer lock for reading. Multiple readers can hold a
 * readers-writer lock at any one time, however if there are any writers
 * waiting for the lock, the function will block and allow the writer to take
 * the lock, in order to prevent starvation of writers.
 *
 * @param lock          Lock to acquire.
 */
void rwlock_read_lock(rwlock_t *lock) {
    rwlock_read_lock_etc(lock, -1, 0);
}

/**
 * Acquires a readers-writer lock for writing. When the lock has been acquired,
 * no other readers or writers will be holding the lock, or be able to acquire
 * it.
 *
 * @param lock          Lock to acquire.
 */
void rwlock_write_lock(rwlock_t *lock) {
    rwlock_write_lock_etc(lock, -1, 0);
}

/** Releases a readers-writer lock.
 * @param lock          Lock to release. */
void rwlock_unlock(rwlock_t *lock) {
    spinlock_lock(&lock->lock);

    if (!lock->held) {
        fatal("Unlock of unheld rwlock %s (%p)", lock->name, lock);
    } else if (!lock->readers || !--lock->readers) {
        rwlock_transfer_ownership(lock);
    }

    spinlock_unlock(&lock->lock);
}

/** Initializes a readers-writer lock.
 * @param lock          Lock to initialize.
 * @param name          Name to give lock. */
void rwlock_init(rwlock_t *lock, const char *name) {
    spinlock_init(&lock->lock, "rwlock_lock");
    list_init(&lock->waiters);

    lock->held    = 0;
    lock->readers = 0;
    lock->name    = name;
}
