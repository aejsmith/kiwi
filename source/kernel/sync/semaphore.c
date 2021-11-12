/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Semaphore implementation.
 */

#include <kernel/semaphore.h>

#include <lib/notifier.h>

#include <mm/malloc.h>

#include <proc/process.h>

#include <sync/semaphore.h>

#include <assert.h>
#include <cpu.h>
#include <object.h>
#include <kernel.h>
#include <status.h>

/** User semaphore structure. */
typedef struct user_semaphore {
    semaphore_t sem;                /**< Semaphore implementation. */
    notifier_t notifier;            /**< Notifier for semaphore events. */
} user_semaphore_t;

/** Decreases the count of a semaphore.
 * @param sem           Semaphore to down.
 * @param timeout       Timeout in nanoseconds. If SLEEP_ABSOLUTE is specified,
 *                      will always be taken to be a system time at which the
 *                      sleep will time out. Otherwise, taken as the number of
 *                      nanoseconds in which the sleep will time out. If 0 is
 *                      specified, the function will return an error immediately
 *                      if the semaphore's count is currently zero. If -1
 *                      is specified, the thread will sleep indefinitely until
 *                      the semaphore can be downed or it is interrupted.
 * @param flags         Sleeping behaviour flags.
 * @return              Status code describing result of the operation. Failure
 *                      is only possible if the timeout is not -1, or if the
 *                      SLEEP_INTERRUPTIBLE flag is set. */
status_t semaphore_down_etc(semaphore_t *sem, nstime_t timeout, unsigned flags) {
    assert(!in_interrupt());

    spinlock_lock(&sem->lock);

    if (sem->count) {
        --sem->count;
        spinlock_unlock(&sem->lock);
        return STATUS_SUCCESS;
    }

    list_append(&sem->threads, &curr_thread->wait_link);
    return thread_sleep(&sem->lock, timeout, sem->name, flags);
}

/** Decreases the count of a semaphore.
 * @param sem           Semaphore to down. */
void semaphore_down(semaphore_t *sem) {
    semaphore_down_etc(sem, -1, 0);
}

/** Increases the count of a semaphore.
 * @param sem           Semaphore to up.
 * @param count         Value to increment the count by. */
void semaphore_up(semaphore_t *sem, size_t count) {
    spinlock_lock(&sem->lock);

    for (size_t i = 0; i < count; i++) {
        if (list_empty(&sem->threads)) {
            sem->count++;
        } else {
            thread_t *thread = list_first(&sem->threads, thread_t, wait_link);
            thread_wake(thread);
        }
    }

    spinlock_unlock(&sem->lock);
}

/** Initializes a semaphore structure.
 * @param sem           Semaphore to initialize.
 * @param name          Name of the semaphore, for debugging purposes.
 * @param initial       Initial value of the semaphore. */
void semaphore_init(semaphore_t *sem, const char *name, size_t initial) {
    spinlock_init(&sem->lock, "semaphore_lock");
    list_init(&sem->threads);

    sem->count = initial;
    sem->name  = name;
}

/**
 * User semaphore API.
 */

/** Closes a handle to a semaphore. */
static void semaphore_object_close(object_handle_t *handle) {
    user_semaphore_t *sem = handle->private;

    notifier_clear(&sem->notifier);
    kfree(sem);
}

/** Signal that a semaphore is being waited for. */
static status_t semaphore_object_wait(object_handle_t *handle, object_event_t *event) {
    user_semaphore_t *sem = handle->private;

    switch (event->event) {
        case SEMAPHORE_EVENT:
            if (!(event->flags & OBJECT_EVENT_EDGE) && sem->sem.count) {
                object_event_signal(event, 0);
            } else {
                notifier_register(&sem->notifier, object_event_notifier, event);
            }

            return STATUS_SUCCESS;
        default:
            return STATUS_INVALID_EVENT;
    }
}

/** Stop waiting for a semaphore. */
static void semaphore_object_unwait(object_handle_t *handle, object_event_t *event) {
    user_semaphore_t *sem = handle->private;

    switch (event->event) {
        case SEMAPHORE_EVENT:
            notifier_unregister(&sem->notifier, object_event_notifier, event);
            break;
    }
}

/** Semaphore object type. */
static const object_type_t semaphore_object_type = {
    .id     = OBJECT_TYPE_SEMAPHORE,
    .flags  = OBJECT_TRANSFERRABLE,
    .close  = semaphore_object_close,
    .wait   = semaphore_object_wait,
    .unwait = semaphore_object_unwait,
};

/** Creates a semaphore object.
 * @param count         Initial value of the semaphore.
 * @param _handle       Where to store handle to semaphore.
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NO_HANDLES if handle table is full. */
status_t kern_semaphore_create(size_t count, handle_t *_handle) {
    user_semaphore_t *sem = kmalloc(sizeof(*sem), MM_KERNEL);
    semaphore_init(&sem->sem, "user_semaphore", count);
    notifier_init(&sem->notifier, sem);

    status_t ret = object_handle_open(&semaphore_object_type, sem, NULL, _handle);
    if (ret != STATUS_SUCCESS)
        kfree(sem);

    return ret;
}

/**
 * Decreases a semaphore's current count. If the count is currently zero, then
 * the function will block until the count becomes non-zero due to a call to
 * kern_semaphore_up(), and then decreases it again and returns.
 *
 * @param handle        Handle to semaphore.
 * @param timeout       Timeout in nanoseconds. If 0 is specified, the function
 *                      will return an error immediately if the semaphore's
 *                      count is currently zero. If -1 is specified, the
 *                      function will block indefinitely until the semaphore can
 *                      be downed or it is interrupted.
 *
 * @return              STATUS_SUCCESS if downed successfully.
 *                      STATUS_INVALID_HANDLE if handle does not refer to a
 *                      semaphore.
 *                      STATUS_WOULD_BLOCK if timeout is 0 and the semaphore's
 *                      count is currently 0.
 *                      STATUS_TIMED_OUT if timeout is greater than 0 and the
 *                      timeout expires before the semaphore count becomes non-
 *                      zero.
 *                      STATUS_INTERRUPTED if the thread is interrupted while
 *                      waiting.
 */
status_t kern_semaphore_down(handle_t handle, nstime_t timeout) {
    status_t ret;

    object_handle_t *khandle;
    ret = object_handle_lookup(handle, OBJECT_TYPE_SEMAPHORE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    user_semaphore_t *sem = khandle->private;

    ret = semaphore_down_etc(&sem->sem, timeout, SLEEP_INTERRUPTIBLE);

    object_handle_release(khandle);
    return ret;
}

/**
 * Increases the count of a semaphore by the specified value, which may result
 * in threads waiting in kern_semaphore_down() being unblocked.
 *
 * @param handle        Handle to semaphore.
 * @param count         Value to increase count by.
 * @param prev          Where to store previous count value.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_INVALID_HANDLE if handle does not refer to a
 *                      semaphore.
 *                      STATUS_INVALID_ARG if count is 0.
 *                      STATUS_OVERFLOW if the semaphore's count would overflow
 *                      (in this case no threads are woken).
 */
status_t kern_semaphore_up(handle_t handle, size_t count) {
    if (!count)
        return STATUS_INVALID_ARG;

    object_handle_t *khandle;
    status_t ret = object_handle_lookup(handle, OBJECT_TYPE_SEMAPHORE, &khandle);
    if (ret != STATUS_SUCCESS)
        return ret;

    user_semaphore_t *sem = khandle->private;

    spinlock_lock(&sem->sem.lock);

    /* Count the number of threads we would wake. */
    size_t num_threads = 0;
    list_foreach(&sem->sem.threads, iter) {
        if (++num_threads == count)
            break;
    }

    /* Check for overflow. */
    if (sem->sem.count + (count - num_threads) < sem->sem.count) {
        spinlock_lock(&sem->sem.lock);
        object_handle_release(khandle);
        return STATUS_OVERFLOW;
    }

    /* Wake them up. */
    for (size_t i = 0; i < num_threads; i++) {
        thread_t *thread = list_first(&sem->sem.threads, thread_t, wait_link);
        thread_wake(thread);
    }

    /* Add any remainder onto the count. */
    sem->sem.count += count - num_threads;

    spinlock_unlock(&sem->sem.lock);

    if (count - num_threads)
        notifier_run(&sem->notifier, NULL, false);

    object_handle_release(khandle);
    return STATUS_SUCCESS;
}
