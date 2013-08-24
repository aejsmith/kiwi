/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		POSIX threads.
 */

#ifndef __PTHREAD_H
#define __PTHREAD_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __DUMMY_PTHREADS

typedef void *pthread_t;

typedef int32_t pthread_key_t;
typedef int32_t pthread_once_t;
typedef int32_t pthread_mutex_t;
typedef int32_t pthread_cond_t;
typedef int32_t pthread_mutexattr_t;

#define PTHREAD_ONCE_INIT 0
#define PTHREAD_MUTEX_INITIALIZER 0
#define PTHREAD_COND_INITIALIZER 0

static inline pthread_t pthread_self(void) { return (void *)0xdeadbeef; }
static inline int pthread_equal(pthread_t p1, pthread_t p2) { return (p1 == p2); }

extern int pthread_key_create(pthread_key_t *, void (*)(void*));
extern int pthread_setspecific(pthread_key_t, const void *);
extern void *pthread_getspecific(pthread_key_t);
extern int pthread_once(pthread_once_t *, void (*)(void));
extern int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
extern int pthread_mutex_lock(pthread_mutex_t *);
extern int pthread_mutex_unlock(pthread_mutex_t *);
extern int pthread_cond_wait(pthread_cond_t *__restrict, pthread_mutex_t *__restrict);
extern int pthread_cond_signal(pthread_cond_t *);

// shouldn't be here.
static inline int sched_yield(void) { return 0; }

#endif

#ifdef __cplusplus
}
#endif

#endif /* __PTHREAD_H */
