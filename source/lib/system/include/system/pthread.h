/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX thread types.
 *
 * TODO:
 *  - Make all of these opaque structures?
 */

#pragma once

#include <core/mutex.h>

#include <kernel/types.h>

__SYS_EXTERN_C_BEGIN

struct __pthread;

/** Type of a control variable for pthread_once(). */
typedef int32_t pthread_once_t;

/** Type of a key for thread-local data. */
typedef int32_t pthread_key_t;

/** Structure containing mutex attributes. */
typedef struct {
    int type;                       /**< Type of the mutex. */
    int pshared;                    /**< Process sharing attribute. */
} pthread_mutexattr_t;

/** Structure containing a mutex. */
typedef struct {
    volatile int32_t futex;         /**< Futex implementing the lock. */
    thread_id_t holder;             /**< ID of holding thread. */
    unsigned recursion;             /**< Recursion count. */
    pthread_mutexattr_t attr;       /**< Attributes for the mutex. */
} pthread_mutex_t;

// TODO
typedef pthread_mutex_t pthread_rwlock_t;

/** Structure containing condition variable attributes. */
typedef struct {
    int pshared;                    /**< Process sharing attribute. */
} pthread_condattr_t;

/** Structure containing a condition variable. */
typedef struct {
    core_mutex_t lock;              /**< Internal structure lock. */
    uint32_t futex;                 /**< Futex to wait on. */
    uint32_t waiters;               /**< Number of waiters. */
    pthread_mutex_t *mutex;         /**< Mutex being used with the condition. */
    pthread_condattr_t attr;        /**< Attributes for the condition variable. */
} pthread_cond_t;

/** Type of a POSIX thread handle. */
typedef struct __pthread *pthread_t;

/** Type of a thread attributes object (TODO). */
typedef int32_t pthread_attr_t;

__SYS_EXTERN_C_END
