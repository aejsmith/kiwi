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

/**
 * Transfers lock ownership to a waiting writer or waiting readers. Queue lock
 * should be held.
 */
static void rwlock_transfer_ownership(rwlock_t *lock) {
    /* Check if there are any threads to transfer ownership to. */
    if (list_empty(&lock->threads)) {
        /* There aren't. If there are still readers (it is possible for there to
         * be, because this function gets called if a writer is interrupted
         * while blocking in order to allow readers queued behind it in), then
         * we do not want to do anything. Otherwise, release the lock. */
        if (!lock->readers)
            lock->held = 0;
    } else {
        /* Go through all threads queued. */
        list_foreach_safe(&lock->threads, iter) {
            thread_t *thread = list_entry(iter, thread_t, wait_link);

            /* If it is a reader, we can wake it and continue. If it is a writer
             * and the lock has no readers, wake it up and finish. If it is a
             * writer and the lock has readers, finish. */
            if (thread->flags & THREAD_RWLOCK_WRITER && lock->readers) {
                break;
            } else {
                thread_wake(thread);

                if (thread->flags & THREAD_RWLOCK_WRITER) {
                    break;
                } else {
                    lock->readers++;
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
status_t rwlock_read_lock_etc(rwlock_t *lock, nstime_t timeout, unsigned flags) {
    assert(!in_interrupt());

    spinlock_lock(&lock->lock);

    if (lock->held) {
        /* Lock is held, check if it's held by readers. If it is, and there's
         * something waiting on the queue, we wait anyway. This is to prevent
         * starvation of writers. */
        if (!lock->readers || !list_empty(&lock->threads)) {
            /* Readers count will have been incremented for us upon success. */
            list_append(&lock->threads, &curr_thread->wait_link);
            return thread_sleep(&lock->lock, timeout, lock->name, flags);
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
status_t rwlock_write_lock_etc(rwlock_t *lock, nstime_t timeout, unsigned flags) {
    assert(!in_interrupt());

    status_t ret = STATUS_SUCCESS;

    spinlock_lock(&lock->lock);

    /* Just acquire the exclusive lock. */
    if (lock->held) {
        curr_thread->flags |= THREAD_RWLOCK_WRITER;
        list_append(&lock->threads, &curr_thread->wait_link);
        ret = thread_sleep(&lock->lock, timeout, lock->name, flags);
        curr_thread->flags &= ~THREAD_RWLOCK_WRITER;

        if (ret != STATUS_SUCCESS) {
            /* Failed to acquire the lock. In this case, there may be a reader
             * queued behind us that can be let in. */
            spinlock_lock(&lock->lock);
            if (lock->readers)
                rwlock_transfer_ownership(lock);
            spinlock_unlock(&lock->lock);
        }
    } else {
        lock->held = 1;
        spinlock_unlock(&lock->lock);
    }

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
    list_init(&lock->threads);

    lock->held    = 0;
    lock->readers = 0;
    lock->name    = name;
}
