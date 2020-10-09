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
 * @brief               Spinlock implementation.
 */

#pragma once

#include <lib/atomic.h>

/** Structure containing a spinlock. */
typedef struct spinlock {
    atomic_int value;           /**< Value of lock (1 == free, 0 == held, others == held with waiters). */
    volatile bool state;        /**< Interrupt state prior to locking. */
    const char *name;           /**< Name of the spinlock. */
} spinlock_t;

/** Initializes a statically defined spinlock. */
#define SPINLOCK_INITIALIZER(_name) \
    { \
        .value = 1, \
        .state = 0, \
        .name = _name, \
    }

/** Statically defines a new spinlock. */
#define SPINLOCK_DEFINE(_var) \
    spinlock_t _var = SPINLOCK_INITIALIZER(#_var)

/** Check if a spinlock is held.
 * @param lock          Spinlock to check.
 * @return              True if lock is locked, false otherwise. */
static inline bool spinlock_held(spinlock_t *lock) {
    return atomic_load_explicit(&lock->value, memory_order_relaxed) != 1;
}

extern void spinlock_lock(spinlock_t *lock);
extern void spinlock_lock_noirq(spinlock_t *lock);
extern void spinlock_unlock(spinlock_t *lock);
extern void spinlock_unlock_noirq(spinlock_t *lock);

extern void spinlock_init(spinlock_t *lock, const char *name);
