/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Readers-writer lock implementation.
 */

#pragma once

#include <lib/list.h>

#include <sync/spinlock.h>

/** Structure containing a readers-writer lock. */
typedef struct rwlock {
    unsigned held;                  /**< Whether the lock is held. */
    size_t readers;                 /**< Number of readers holding the lock. */
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t waiters;                 /**< List of waiting threads. */
    const char *name;               /**< Name of the lock. */
} rwlock_t;

/** Initializes a statically defined readers-writer lock. */
#define RWLOCK_INITIALIZER(_var, _name) \
    { \
        .held = 0, \
        .readers = 0, \
        .lock = SPINLOCK_INITIALIZER("rwlock_lock"), \
        .waiters = LIST_INITIALIZER(_var.waiters), \
        .name = _name, \
    }

/** Statically defines a new readers-writer lock. */
#define RWLOCK_DEFINE(_var) \
    rwlock_t _var = RWLOCK_INITIALIZER(_var, #_var)

extern status_t rwlock_read_lock_etc(rwlock_t *lock, nstime_t timeout, uint32_t flags);
extern status_t rwlock_write_lock_etc(rwlock_t *lock, nstime_t timeout, uint32_t flags);
extern void rwlock_read_lock(rwlock_t *lock);
extern void rwlock_write_lock(rwlock_t *lock);
extern void rwlock_unlock(rwlock_t *lock);

extern void rwlock_init(rwlock_t *lock, const char *name);
