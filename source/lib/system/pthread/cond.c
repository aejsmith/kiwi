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
 * @brief               POSIX condition variable functions.
 *
 * This is *incredibly* difficult to get right without race conditions. I
 * wouldn't be surprised at all if there's still some in here.
 */

#include <kernel/futex.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>

#include "libsystem.h"

/** Initialize a condition variable.
 * @param cond          Condition variable to initialize. Attempting to
 *                      initialize an already initialized condition variable
 *                      results in undefined behaviour.
 * @param attr          Optional attributes structure. If NULL, default
 *                      attributes will be used.
 * @return              0 on success, error number on failure. */
int pthread_cond_init(pthread_cond_t *__restrict cond, const pthread_condattr_t *__restrict attr) {
    cond->lock = CORE_MUTEX_INITIALIZER;
    cond->futex = 0;
    cond->mutex = NULL;
    cond->waiters = 0;

    if (attr) {
        cond->attr = *attr;
    } else {
        cond->attr.pshared = PTHREAD_PROCESS_PRIVATE;
    }

    return 0;
}

/** Destroy a condition variable.
 * @param cond          Condition variable to destroy. Attempting to destroy a
 *                      condition variable upon which other threads are blocked
 *                      results in undefined behaviour.
 * @return              Always returns 0. */
int pthread_cond_destroy(pthread_cond_t *cond) {
    /* libcxx is currently configured to not call this since it is trivial, if
     * this changes, update libcxx accordingly. */

    if (cond->waiters != 0)
        libsystem_fatal("destroying condition variable %p with waiters", cond);

    return 0;
}

/**
 * Block on a condition variable.
 *
 * Atomically releases the specified mutex and blocks the current thread on a
 * condition variable. Atomically means that if another thread acquires the
 * mutex after a thread that is about to block has released it, a call to
 * pthread_cond_signal() or pthread_cond_broadcast() shall behave as if the
 * thread has blocked.
 *
 * When a thread waits on a condition variable having specified a particular
 * mutex, a binding is formed between the condition variable and the mutex
 * which remains in place until no more threads are blocked on the condition
 * variable. A thread which attempts to block specifying a different mutex
 * while this binding is in place will result in undefined behaviour.
 *
 * If the calling thread does not hold the specified mutex and it is of type
 * PTHREAD_MUTEX_ERRORCHECK, an error will be returned. Otherwise, behaviour
 * if the mutex is unheld is undefined.
 *
 * Spurious wakeups can occur with this function, i.e. more than one thread
 * may wake as a result of a call to pthread_cond_signal(). To handle this,
 * applications are expected to wrap a condition wait in a loop testing the
 * condition predicate.
 *
 * @param cond          Condition variable to block on.
 * @param mutex         Mutex to release.
 *
 * @return              0 if the thread successfully blocked and was woken by a
 *                      call to pthread_cond_signal() or pthread_cond_broadcast().
 *                      EPERM if mutex is of type PTHREAD_MUTEX_ERRORCHECK and
 *                      the calling thread does not hold it.
 */
int pthread_cond_wait(pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex) {
    uint32_t val;
    status_t ret;

    /* POSIX doesn't seem to specify anything about using recursive mutexes
     * with condition variables, so I'm taking that to mean that I can throw an
     * error. */
    if (mutex->attr.type == PTHREAD_MUTEX_RECURSIVE)
        libsystem_fatal("using recursive mutex %p with condition %p", mutex, cond);

    thread_id_t self;
    kern_thread_id(THREAD_SELF, &self);

    if (mutex->holder != self) {
        if (mutex->attr.type == PTHREAD_MUTEX_ERRORCHECK)
            return EPERM;

        libsystem_fatal("using unheld mutex %p with condition %p", mutex, cond);
    }

    core_mutex_lock(&cond->lock, -1);

    /* Can't do mutex checking if this is a process-shared condition variable,
     * as the mutex address may be different. */
    if (cond->attr.pshared != PTHREAD_PROCESS_SHARED) {
        if (cond->mutex && cond->mutex != mutex) {
            libsystem_fatal(
                "incorrect mutex %p used with condition %p, expected %p",
                mutex, cond, cond->mutex);
        }

        cond->mutex = mutex;
    }

    cond->waiters++;

    /* Drop the mutex. */
    pthread_mutex_unlock(mutex);

    /* Save the futex value, then attempt to wait. This guarantees atomicity:
     * any wakeup event results in a change in the futex value. If a wakeup
     * occurs between unlocking and sleeping, it will be picked up by
     * kern_futex_wait() and it will return immediately. You may notice that
     * this means that a call to pthread_cond_signal() can cause multiple
     * wakeups: if the value changes here, we will return from kern_futex_wait()
     * immediately without sleeping, but a thread that is already sleeping on
     * the futex will be woken as well. This is specifically allowed by POSIX.
     * Applications should be waiting within a loop testing the condition
     * predicate. */
    val = cond->futex;
    core_mutex_unlock(&cond->lock);
    kern_futex_wait((int32_t *)&cond->futex, val, -1);
    core_mutex_lock(&cond->lock, -1);

    /* If there are no more waiters, set mutex to NULL. */
    if (--cond->waiters == 0) {
        if (cond->attr.pshared != PTHREAD_PROCESS_SHARED)
            cond->mutex = NULL;
    }

    core_mutex_unlock(&cond->lock);

    /* Relock the mutex. */
    while (__sync_lock_test_and_set(&mutex->futex, 2) != 0) {
        ret = kern_futex_wait((int32_t *)&mutex->futex, 2, -1);
        if (ret != STATUS_SUCCESS && ret != STATUS_TRY_AGAIN) {
            /* FIXME: Not correct, we're supposed to return with the mutex
             * locked. But what else can we do? */
            libsystem_status_to_errno(ret);
            return errno;
        }
    }

    mutex->holder    = self;
    mutex->recursion = 1;
    return 0;
}

/**
 * Block on a condition variable.
 *
 * Atomically releases the specified mutex and blocks the current thread on a
 * condition variable. Atomically means that if another thread acquires the
 * mutex after a thread that is about to block has released it, a call to
 * pthread_cond_signal() or pthread_cond_broadcast() shall behave as if the
 * thread has blocked.
 *
 * When a thread waits on a condition variable having specified a particular
 * mutex, a binding is formed between the condition variable and the mutex
 * which remains in place until no more threads are blocked on the condition
 * variable. A thread which attempts to block specifying a different mutex
 * while this binding is in place will result in undefined behaviour.
 *
 * If the calling thread does not hold the specified mutex and it is of type
 * PTHREAD_MUTEX_ERRORCHECK, an error will be returned. Otherwise, behaviour
 * if the mutex is unheld is undefined.
 *
 * Spurious wakeups can occur with this function, i.e. more than one thread
 * may wake as a result of a call to pthread_cond_signal(). To handle this,
 * applications are expected to wrap a condition wait in a loop testing the
 * condition predicate.
 *
 * @param cond          Condition variable to block on.
 * @param mutex         Mutex to release.
 * @param abstime       Absolute time (measured against the clock specified by
 *                      the condition variable's clock attribute) at which the
 *                      wait will time out.
 *
 * @return              0 if the thread successfully blocked and was woken by a
 *                      call to pthread_cond_signal() or pthread_cond_broadcast().
 *                      EPERM if mutex is of type PTHREAD_MUTEX_ERRORCHECK and
 *                      the calling thread does not hold it.
 *                      ETIMEDOUT if the time specified by abstime has passed
 *                      without the thread being woken by pthread_cond_signal()
 *                      or pthread_cond_broadcast().
 */
int pthread_cond_timedwait(
    pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex,
    const struct timespec *__restrict abstime)
{
    libsystem_stub("pthread_cond_timedwait", true);
    return ENOSYS;
}

/** Unblock all threads blocked on a condition variable.
 * @param cond          Condition variable to broadcast.
 * @return              0 on success, error code on failure. */
int pthread_cond_broadcast(pthread_cond_t *cond) {
    status_t ret;

    core_mutex_lock(&cond->lock, -1);

    /* Increment the futex to signal that there's a wakeup event. Note that the
     * actual futex value is irrelevant. It can wrap around without issue. It is
     * just compared in pthread_cond_wait() to see if it has changed. */
    cond->futex++;

    if (cond->attr.pshared != PTHREAD_PROCESS_SHARED && cond->mutex) {
        /* Wake one waiter and requeue the remainder on the mutex. In this case
         * the futex value cannot change under us as we hold the internal lock,
         * so don't need to check for STATUS_TRY_AGAIN. */
        ret = kern_futex_requeue(
            (int32_t *)&cond->futex, cond->futex, 1,
            (int32_t *)&cond->mutex->futex, NULL);
    } else {
        /* Cannot use requeue for shared conditions as we don't know the mutex. */
        ret = kern_futex_wake((int32_t *)&cond->futex, ~0UL, NULL);
    }

    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        core_mutex_unlock(&cond->lock);
        return errno;
    }

    core_mutex_unlock(&cond->lock);
    return 0;
}

/** Unblock a single thread blocked on a condition variable.
 * @param cond          Condition variable to signal.
 * @return              0 on success, error code on failure. */
int pthread_cond_signal(pthread_cond_t *cond) {
    status_t ret;

    core_mutex_lock(&cond->lock, -1);

    /* Same as above. */
    cond->futex++;

    /* Wake only one waiter. */
    ret = kern_futex_wake((int32_t *)&cond->futex, 1, NULL);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        core_mutex_unlock(&cond->lock);
        return errno;
    }

    core_mutex_unlock(&cond->lock);
    return 0;
}

/** Initialize a condition variable attributes structure with default values.
 * @param attr          Attributes structure to initialize.
 * @return              Always returns 0. */
int pthread_condattr_init(pthread_condattr_t *attr) {
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

/** Destroy a condition variable attributes structure.
 * @param attr          Attributes structure to destroy.
 * @return              Always returns 0. */
int pthread_condattr_destroy(pthread_condattr_t *attr) {
    /* Nothing to do. */
    return 0;
}

/** Get the value of the process-shared attribute.
 * @param attr          Attributes structure to get from.
 * @param psharedp      Where to store value of attribute.
 * @return              Always returns 0. */
int pthread_condattr_getpshared(const pthread_condattr_t *__restrict attr, int *__restrict psharedp) {
    *psharedp = attr->pshared;
    return 0;
}

/** Set the value of the process-shared attribute.
 * @param attr          Attributes structure to set in.
 * @param pshared       New value of the attribute.
 * @return              0 on success, EINVAL if new value is invalid. */
int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared) {
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;

    attr->pshared = pshared;
    return 0;
}
