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
 * @brief               POSIX threads.
 */

#pragma once

#include <system/pthread.h>

#include <sched.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Process sharing attributes. */
enum {
    /** Object is only used within the current process. */
    PTHREAD_PROCESS_PRIVATE,

    /** Object can be shared between other processes using shared memory. */
    PTHREAD_PROCESS_SHARED,
};

/** Mutex type attribute values. */
enum {
    /** Normal behaviour. */
    PTHREAD_MUTEX_NORMAL,

    /** Perform additional error checks. */
    PTHREAD_MUTEX_ERRORCHECK,

    /** Allow recursive locking by the holding thread. */
    PTHREAD_MUTEX_RECURSIVE,

    /** Implementation-defined default behaviour. */
    PTHREAD_MUTEX_DEFAULT,
};

/** Initializer for pthread_once_t. */
#define PTHREAD_ONCE_INIT 0

/** Default initializer for pthread_mutex_t. */
#define PTHREAD_MUTEX_INITIALIZER \
    { 0, -1, 0, { PTHREAD_MUTEX_DEFAULT, PTHREAD_PROCESS_PRIVATE } }

// TODO
#define PTHREAD_RWLOCK_INITIALIZER \
    PTHREAD_MUTEX_INITIALIZER

/** Default initializer for pthread_cond_t. */
#define PTHREAD_COND_INITIALIZER \
    { 0, 0, 0, NULL, { PTHREAD_PROCESS_PRIVATE } }

//extern int pthread_atfork(void (*prepare)(void), void (*parent)(void),
//  void (*child)(void));
//int pthread_cancel(pthread_t);
//void pthread_cleanup_pop(int);
//void pthread_cleanup_push(void (*)(void*), void *);
//int pthread_create(pthread_t *__restrict, const pthread_attr_t *__restrict,
//  void *(*)(void*), void *__restrict);
//int pthread_detach(pthread_t);
extern int pthread_equal(pthread_t p1, pthread_t p2);
//void pthread_exit(void *);
//int pthread_getschedparam(pthread_t, int *__restrict, struct sched_param *__restrict);
//int pthread_join(pthread_t, void **);
extern int pthread_once(pthread_once_t *once, void (*func)(void));
extern pthread_t pthread_self(void);
//int pthread_setcancelstate(int, int *);
//int pthread_setcanceltype(int, int *);
//int pthread_setschedparam(pthread_t, int, const struct sched_param *);
//int pthread_setschedprio(pthread_t, int);
//void pthread_testcancel(void);

//int pthread_attr_destroy(pthread_attr_t *);
//int pthread_attr_getdetachstate(const pthread_attr_t *, int *);
//int pthread_attr_getguardsize(const pthread_attr_t *__restrict, size_t *__restrict);
//int pthread_attr_getinheritsched(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_getschedparam(const pthread_attr_t *__restrict, struct sched_param *__restrict);
//int pthread_attr_getschedpolicy(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_getscope(const pthread_attr_t *__restrict, int *__restrict);
//int pthread_attr_getstack(const pthread_attr_t *__restrict, void **__restrict, size_t *__restrict);
//int pthread_attr_getstacksize(const pthread_attr_t *__restrict, size_t *__restrict);
//int pthread_attr_init(pthread_attr_t *);
//int pthread_attr_setdetachstate(pthread_attr_t *, int);
//int pthread_attr_setguardsize(pthread_attr_t *, size_t);
//int pthread_attr_setinheritsched(pthread_attr_t *, int);
//int pthread_attr_setschedparam(pthread_attr_t *__restrict, const struct sched_param *__restrict);
//int pthread_attr_setschedpolicy(pthread_attr_t *, int);
//int pthread_attr_setscope(pthread_attr_t *, int);
//int pthread_attr_setstack(pthread_attr_t *, void *, size_t);
//int pthread_attr_setstacksize(pthread_attr_t *, size_t);

//int pthread_barrier_init(
//  pthread_barrier_t *__restrict, const pthread_barrierattr_t *__restrict,
//  unsigned);
//int pthread_barrier_destroy(pthread_barrier_t *);
//int pthread_barrier_wait(pthread_barrier_t *);
//int pthread_barrierattr_init(pthread_barrierattr_t *);
//int pthread_barrierattr_destroy(pthread_barrierattr_t *);
//int pthread_barrierattr_getpshared(const pthread_barrierattr_t *__restrict, int *__restrict);
//int pthread_barrierattr_setpshared(pthread_barrierattr_t *, int);

extern int pthread_cond_init(
    pthread_cond_t *__restrict cond,
    const pthread_condattr_t *__restrict attr);
extern int pthread_cond_destroy(pthread_cond_t *cond);
extern int pthread_cond_wait(pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex);
extern int pthread_cond_timedwait(
    pthread_cond_t *__restrict cond, pthread_mutex_t *__restrict mutex,
    const struct timespec *__restrict abstime);
extern int pthread_cond_broadcast(pthread_cond_t *cond);
extern int pthread_cond_signal(pthread_cond_t *cond);
extern int pthread_condattr_init(pthread_condattr_t *attr);
extern int pthread_condattr_destroy(pthread_condattr_t *attr);
//int pthread_condattr_getclock(const pthread_condattr_t *__restrict, clockid_t *__restrict);
extern int pthread_condattr_getpshared(
    const pthread_condattr_t *__restrict attr,
    int *__restrict psharedp);
//int pthread_condattr_setclock(pthread_condattr_t *, clockid_t);
extern int pthread_condattr_setpshared(pthread_condattr_t *attr, int pshared);

extern int pthread_key_create(pthread_key_t *_key, void (*dtor)(void *val));
extern int pthread_key_delete(pthread_key_t key);
extern void *pthread_getspecific(pthread_key_t key);
extern int pthread_setspecific(pthread_key_t key, const void *val);

extern int pthread_mutex_init(
    pthread_mutex_t *__restrict mutex,
    const pthread_mutexattr_t *__restrict attr);
extern int pthread_mutex_destroy(pthread_mutex_t *mutex);
//int pthread_mutex_getprioceiling(const pthread_mutex_t *__restrict, int *__restrict);
//int pthread_mutex_consistent(pthread_mutex_t *);
extern int pthread_mutex_lock(pthread_mutex_t *mutex);
//int pthread_mutex_setprioceiling(pthread_mutex_t *__restrict, int, int *__restrict);
//extern int pthread_mutex_timedlock(
//  pthread_mutex_t *__restrict mutex,
//  const struct timespec *__restrict abstime);
extern int pthread_mutex_trylock(pthread_mutex_t *mutex);
extern int pthread_mutex_unlock(pthread_mutex_t *mutex);
extern int pthread_mutexattr_init(pthread_mutexattr_t *attr);
extern int pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
//int pthread_mutexattr_getprioceiling(const pthread_mutexattr_t *__restrict, int *__restrict);
//int pthread_mutexattr_getprotocol(const pthread_mutexattr_t *__restrict, int *__restrict);
extern int pthread_mutexattr_getpshared(
    const pthread_mutexattr_t *__restrict attr,
    int *__restrict psharedp);
//int pthread_mutexattr_getrobust(const pthread_mutexattr_t *__restrict, int *__restrict);
extern int pthread_mutexattr_gettype(
    const pthread_mutexattr_t *__restrict attr,
    int *__restrict typep);
//int pthread_mutexattr_setprioceiling(pthread_mutexattr_t *, int);
//int pthread_mutexattr_setprotocol(pthread_mutexattr_t *, int);
extern int pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);
//int pthread_mutexattr_setrobust(pthread_mutexattr_t *, int);
extern int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);

//int pthread_rwlock_init(pthread_rwlock_t *__restrict, const pthread_rwlockattr_t *__restrict);
//int pthread_rwlock_destroy(pthread_rwlock_t *);
int pthread_rwlock_rdlock(pthread_rwlock_t *);
//int pthread_rwlock_timedrdlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
//int pthread_rwlock_timedwrlock(pthread_rwlock_t *__restrict, const struct timespec *__restrict);
//int pthread_rwlock_tryrdlock(pthread_rwlock_t *);
//int pthread_rwlock_trywrlock(pthread_rwlock_t *);
int pthread_rwlock_unlock(pthread_rwlock_t *);
int pthread_rwlock_wrlock(pthread_rwlock_t *);
//int pthread_rwlockattr_init(pthread_rwlockattr_t *);
//int pthread_rwlockattr_destroy(pthread_rwlockattr_t *);
//int pthread_rwlockattr_getpshared(const pthread_rwlockattr_t *__restrict, int *__restrict);
//int pthread_rwlockattr_setpshared(pthread_rwlockattr_t *, int);

//int pthread_spin_destroy(pthread_spinlock_t *);
//int pthread_spin_init(pthread_spinlock_t *, int);
//int pthread_spin_lock(pthread_spinlock_t *);
//int pthread_spin_trylock(pthread_spinlock_t *);
//int pthread_spin_unlock(pthread_spinlock_t *);

#ifdef __cplusplus

extern int pthread_create(
    pthread_t *__restrict, const pthread_attr_t *__restrict,
    void *(*)(void*), void *__restrict);
extern int pthread_detach(pthread_t);
extern int pthread_join(pthread_t, void **);

#endif

#ifdef __cplusplus
}
#endif
