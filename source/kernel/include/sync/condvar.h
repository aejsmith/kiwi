/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Condition variable implementation.
 */

#pragma once

#include <sync/mutex.h>
#include <sync/spinlock.h>

/** Structure containing a condition variable. */
typedef struct condvar {
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t threads;                 /**< List of waiting threads. */
    const char *name;               /**< Name of the condition variable. */
} condvar_t;

/** Initializes a statically defined condition variable. */
#define CONDVAR_INITIALIZER(_var, _name) \
    { \
        .lock = SPINLOCK_INITIALIZER("condvar_lock"), \
        .threads = LIST_INITIALIZER(_var.threads), \
        .name = _name, \
    }

/** Statically defines a new condition variable. */
#define CONDVAR_DEFINE(_var) \
    condvar_t _var = CONDVAR_INITIALIZER(_var, #_var)

extern status_t condvar_wait_etc(condvar_t *cv, mutex_t *mutex, nstime_t timeout, uint32_t flags);
extern void condvar_wait(condvar_t *cv, mutex_t *mutex);
extern bool condvar_signal(condvar_t *cv);
extern bool condvar_broadcast(condvar_t *cv);

/** Wait for a condition to become true.
 * @see                 condvar_wait().
 * @param cond          Condition to wait for (evaluated each iteration). */
#define condvar_wait_cond(cv, mutex, cond) \
    do { \
        condvar_wait(cv, mutex); \
    } while (!(cond))

/** Wait for a condition to become true.
 * @see                 condvar_wait_etc().
 * @param cond          Condition to wait for (evaluated each iteration). */
#define condvar_wait_cond_etc(cv, mutex, timeout, flags, cond) \
    __extension__ \
    ({ \
        status_t __ret; \
        do { \
            __ret = condvar_wait_etc(cv, mutex, timeout, flags); \
        } while (__ret == STATUS_SUCCESS && !(cond)); \
        __ret; \
    })

extern void condvar_init(condvar_t *cv, const char *name);
