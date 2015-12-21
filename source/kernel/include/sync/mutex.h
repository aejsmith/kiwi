/*
 * Copyright (C) 2008-2013 Alex Smith
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

#ifndef __SYNC_MUTEX_H
#define __SYNC_MUTEX_H

#include <lib/atomic.h>
#include <lib/list.h>

#include <sync/spinlock.h>

struct thread;

/** Structure containing a mutex. */
typedef struct mutex {
    atomic_t value;                 /**< Lock count. */
    unsigned flags;                 /**< Behaviour flags for the mutex. */
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t threads;                 /**< List of waiting threads. */
    struct thread *holder;          /**< Thread holding the lock. */
    const char *name;               /**< Name of the lock. */
    #if CONFIG_DEBUG
        void *caller;               /**< Return address of lock call. */
    #endif
} mutex_t;

/** Initializes a statically defined mutex. */
#define MUTEX_INITIALIZER(_var, _name, _flags) \
    { \
        .value = 0, \
        .flags = _flags, \
        .lock = SPINLOCK_INITIALIZER("mutex_lock"), \
        .threads = LIST_INITIALIZER(_var.threads), \
        .holder = NULL, \
        .name = _name, \
    }

/** Statically defines a new mutex. */
#define MUTEX_DEFINE(_var, _flags) \
    mutex_t _var = MUTEX_INITIALIZER(_var, #_var, _flags)

/** Mutex behaviour flags. */
#define MUTEX_RECURSIVE     (1<<0)  /**< Allow recursive locking by a thread. */

/** Check if a mutex is held.
 * @param lock          Mutex to check.
 * @return              Whether the mutex is held. */
static inline bool mutex_held(mutex_t *lock) {
    return atomic_get(&lock->value) != 0;
}

/** Get the current recursion count of a mutex.
 * @param lock          Mutex to check.
 * @return              Current recursion count of mutex. */
static inline int mutex_recursion(mutex_t *lock) {
    return atomic_get(&lock->value);
}

extern status_t mutex_lock_etc(mutex_t *lock, nstime_t timeout, unsigned flags);
extern void mutex_lock(mutex_t *lock);
extern void mutex_unlock(mutex_t *lock);
extern void mutex_init(mutex_t *lock, const char *name, unsigned flags);

#endif /* __SYNC_MUTEX_H */
