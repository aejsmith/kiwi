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
 */

#pragma once

#include <lib/list.h>

#include <sync/spinlock.h>

/** Structure containing a readers-writer lock. */
typedef struct rwlock {
    unsigned held;                  /**< Whether the lock is held. */
    size_t readers;                 /**< Number of readers holding the lock. */
    spinlock_t lock;                /**< Lock to protect the thread list. */
    list_t threads;                 /**< List of waiting threads. */
    const char *name;               /**< Name of the lock. */
} rwlock_t;

/** Initializes a statically defined readers-writer lock. */
#define RWLOCK_INITIALIZER(_var, _name) \
    { \
        .held = 0, \
        .readers = 0, \
        .lock = SPINLOCK_INITIALIZER("rwlock_lock"), \
        .threads = LIST_INITIALIZER(_var.threads), \
        .name = _name, \
    }

/** Statically defines a new readers-writer lock. */
#define RWLOCK_DEFINE(_var) \
    rwlock_t _var = RWLOCK_INITIALIZER(_var, #_var)

extern status_t rwlock_read_lock_etc(rwlock_t *lock, nstime_t timeout, unsigned flags);
extern status_t rwlock_write_lock_etc(rwlock_t *lock, nstime_t timeout, unsigned flags);
extern void rwlock_read_lock(rwlock_t *lock);
extern void rwlock_write_lock(rwlock_t *lock);
extern void rwlock_unlock(rwlock_t *lock);

extern void rwlock_init(rwlock_t *lock, const char *name);
