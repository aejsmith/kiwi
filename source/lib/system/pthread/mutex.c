/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX mutex functions.
 *
 * This implementation is based around the "Mutex, take 3" implementation in
 * the paper linked below. The futex has 3 states:
 *  - 0 - Unlocked.
 *  - 1 - Locked, no waiters.
 *  - 2 - Locked, one or more waiters.
 *
 * Reference:
 *  - Futexes are Tricky
 *    http://dept-info.labri.fr/~denis/Enseignement/2008-IR/Articles/01-futex.pdf
 *
 * If changing the internal implementation, be sure to change the condition
 * variable implementation as well, as that prods about at the internals of a
 * mutex.
 *
 * TODO:
 *  - Transfer lock ownership to a woken thread? At the moment if a thread
 *    unlocks and then immediately locks again we can starve other threads.
 */

#include <kernel/futex.h>
#include <kernel/status.h>
#include <kernel/thread.h>

#include <assert.h>
#include <errno.h>
#include <inttypes.h>
#include <pthread.h>

#include "libsystem.h"

/** Initialize a mutex.
 * @param mutex         Mutex to initialize. Attempting to initialize an already
 *                      initialized mutex results in undefined behaviour.
 * @param attr          Optional attributes structure. If NULL, default
 *                      attributes will be used.
 * @return              0 on success, error number on failure. */
int pthread_mutex_init(
    pthread_mutex_t *__restrict mutex,
    const pthread_mutexattr_t *__restrict attr)
{
    mutex->futex = 0;
    mutex->holder = -1;
    mutex->recursion = 0;

    if (attr) {
        mutex->attr = *attr;
    } else {
        mutex->attr.type = PTHREAD_MUTEX_DEFAULT;
        mutex->attr.pshared = PTHREAD_PROCESS_PRIVATE;
    }

    return 0;
}

/** Destroy a mutex.
 * @param mutex         Mutex to destroy. Attempting to destroy a held mutex
 *                      results in undefined behaviour.
 * @return              Always returns 0. */
int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    /* libcxx is currently configured to not call this since it is trivial, if
     * this changes, update libcxx accordingly. */

    if (mutex->futex != 0)
        libsystem_fatal("destroying held mutex %p", mutex);

    return 0;
}

/**
 * Lock a mutex.
 *
 * Attempts to lock the specified mutex and blocks until it is able to do so.
 * If the mutex type is PTHREAD_MUTEX_RECURSIVE, and the mutex is already held
 * by the current thread, the recursion count will be increased and the function
 * will succeed straight away. If the mutex type is PTHREAD_MUTEX_ERRORCHECK,
 * the function will perform additional error checking to detect deadlock.
 *
 * @param mutex         Mutex to lock.
 *
 * @return              0 if the mutex was successfully locked.
 *                      EAGAIN if the mutex is of type PTHREAD_MUTEX_RECURSIVE
 *                      and the maximum recursion count has been reached.
 *                      EDEADLK if the mutex is of type PTHREAD_MUTEX_ERRORCHECK
 *                      and the thread already holds the lock.
 */
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    status_t ret;
    int32_t val;

    thread_id_t self;
    kern_thread_id(THREAD_SELF, &self);

    /* If the futex is currently 0 (unlocked), just set it to 1 (locked, no
     * waiters) and return. */
    val = __sync_val_compare_and_swap(&mutex->futex, 0, 1);
    if (val != 0) {
        if (mutex->holder == self) {
            if (mutex->attr.type == PTHREAD_MUTEX_RECURSIVE) {
                /* Already hold it and the mutex is recursive, increase count
                 * and succeed. */
                mutex->recursion++;
                return 0;
            } else if (mutex->attr.type == PTHREAD_MUTEX_ERRORCHECK) {
                /* Error-checking is enabled, we must notify the caller. */
                return EDEADLK;
            } else if (mutex->attr.type == PTHREAD_MUTEX_DEFAULT) {
                /* POSIX specifies that we should deadlock for
                 * PTHREAD_MUTEX_NORMAL, but behaviour is undefined for
                 * PTHREAD_MUTEX_DEFAULT. Therefore, we can throw an error in
                 * this case. */
                libsystem_fatal("recursive locking of mutex %p", mutex);
            }
        }

        /* Set futex to 2 (locked with waiters). */
        if (val != 2)
            val = __sync_lock_test_and_set(&mutex->futex, 2);

        /* Loop until we can acquire the futex. */
        while (val != 0) {
            ret = kern_futex_wait((int32_t *)&mutex->futex, 2, -1);
            if (ret != STATUS_SUCCESS && ret != STATUS_TRY_AGAIN) {
                libsystem_status_to_errno(ret);
                return errno;
            }

            /* We cannot know whether there are waiters or not. Therefore, to be
             * on the safe side, set that there are (see paper linked above). */
            val = __sync_lock_test_and_set(&mutex->futex, 2);
        }
    }

    mutex->holder    = self;
    mutex->recursion = 1;
    return 0;
}

/**
 * Lock a mutex.
 *
 * Attempts to lock the specified mutex, and returns an error immediately if
 * it is currently held by any thread (including the current). If the mutex
 * type is PTHREAD_MUTEX_RECURSIVE, and the mutex is already held by the
 * current thread, the recursion count will be increased and the function will
 * succeed.
 *
 * @param mutex         Mutex to lock.
 *
 * @return              0 if the mutex was successfully locked.
 *                      EBUSY if the mutex is already held.
 *                      EAGAIN if the mutex is of type PTHREAD_MUTEX_RECURSIVE
 *                      and the maximum recursion count has been reached.
 */
int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    thread_id_t self;
    kern_thread_id(THREAD_SELF, &self);

    if (!__sync_bool_compare_and_swap(&mutex->futex, 0, 1)) {
        if (mutex->holder == self && mutex->attr.type == PTHREAD_MUTEX_RECURSIVE) {
            mutex->recursion++;
            return 0;
        }

        return EBUSY;
    }

    mutex->holder    = self;
    mutex->recursion = 1;
    return 0;
}

/**
 * Unlock a mutex.
 *
 * Unlocks the specified mutex. If the mutex is of type PTHREAD_MUTEX_RECURSIVE
 * and the calling thread has locked the mutex multiple times, the mutex will
 * not be released until the recursion count reaches 0. If the current thread
 * does not hold the mutex and the mutex type is PTHREAD_MUTEX_ERRORCHECK or
 * PTHREAD_MUTEX_RECURSIVE, the function will return an error, otherwise the
 * behaviour is undefined.
 *
 * @param mutex         Mutex to unlock.
 *
 * @return              0 if the mutex was successfully unlocked.
 *                      EPERM if the mutex type is PTHREAD_MUTEX_ERRORCHECK
 *                      or PTHREAD_MUTEX_RECURSIVE and the current thread does
 *                      not hold the lock.
 */
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    thread_id_t self;
    kern_thread_id(THREAD_SELF, &self);

    if (mutex->holder != self) {
        if (mutex->attr.type == PTHREAD_MUTEX_ERRORCHECK ||
            mutex->attr.type == PTHREAD_MUTEX_RECURSIVE)
        {
            return EPERM;
        }

        /* Undefined behaviour -> error. */
        if (mutex->holder == -1) {
            libsystem_fatal("releasing unheld mutex %p", mutex);
        } else {
            libsystem_fatal("releasing mutex %p held by %" PRId32, mutex, mutex->holder);
        }
    }

    if (--mutex->recursion > 0) {
        assert(mutex->attr.type == PTHREAD_MUTEX_RECURSIVE);
        return 0;
    }

    mutex->holder = -1;

    if (__sync_fetch_and_sub(&mutex->futex, 1) != 1) {
        /* There were waiters. Wake one up. */
        mutex->futex = 0;
        kern_futex_wake((int32_t *)&mutex->futex, 1, NULL);
    }

    return 0;
}

/** Initialize a mutex attributes structure with default values.
 * @param attr          Attributes structure to initialize.
 * @return              Always returns 0. */
int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    attr->type = PTHREAD_MUTEX_DEFAULT;
    attr->pshared = PTHREAD_PROCESS_PRIVATE;
    return 0;
}

/** Destroy a mutex attributes structure.
 * @param attr          Attributes structure to destroy.
 * @return              Always returns 0. */
int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    /* Nothing to do. */
    return 0;
}

/** Get the value of the process-shared attribute.
 * @param attr          Attributes structure to get from.
 * @param psharedp      Where to store value of attribute.
 * @return              Always returns 0. */
int pthread_mutexattr_getpshared(const pthread_mutexattr_t *__restrict attr, int *__restrict psharedp) {
    *psharedp = attr->pshared;
    return 0;
}

/** Get the value of the type attribute.
 * @param attr          Attributes structure to get from.
 * @param psharedp      Where to store value of attribute.
 * @return              Always returns 0. */
int pthread_mutexattr_gettype(const pthread_mutexattr_t *__restrict attr, int *__restrict typep) {
    *typep = attr->type;
    return 0;
}

/** Set the value of the process-shared attribute.
 * @param attr          Attributes structure to set in.
 * @param pshared       New value of the attribute.
 * @return              0 on success, EINVAL if new value is invalid. */
int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared) {
    if (pshared != PTHREAD_PROCESS_PRIVATE && pshared != PTHREAD_PROCESS_SHARED)
        return EINVAL;

    attr->pshared = pshared;
    return 0;
}

/** Set the value of the type attribute.
 * @param attr          Attributes structure to set in.
 * @param pshared       New value of the attribute.
 * @return              0 on success, EINVAL if new value is invalid. */
int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (type < PTHREAD_MUTEX_NORMAL || type > PTHREAD_MUTEX_DEFAULT)
        return EINVAL;

    attr->type = type;
    return 0;
}

// TODO: Dummy rwlock implementation as a mutex for libunwind

int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    return pthread_mutex_lock(rwlock);
}

int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    return pthread_mutex_lock(rwlock);
}

int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    return pthread_mutex_unlock(rwlock);
}
