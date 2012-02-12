/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Spinlock implementation.
 */

#ifndef __SYNC_SPINLOCK_H
#define __SYNC_SPINLOCK_H

#include <lib/atomic.h>

#include <sync/sync.h>

/** Structure containing a spinlock. */
typedef struct spinlock {
	atomic_t value;			/**< Value of lock (1 == free, 0 == held, others == held with waiters). */
	volatile bool state;		/**< Interrupt state prior to locking. */
	const char *name;		/**< Name of the spinlock. */
} spinlock_t;

/** Initializes a statically-declared spinlock. */
#define SPINLOCK_INITIALIZER(_name)	\
	{ \
		.value = 1, \
		.state = 0, \
		.name = _name, \
	}

/** Statically declares a new spinlock. */
#define SPINLOCK_DECLARE(_var)		\
	spinlock_t _var = SPINLOCK_INITIALIZER(#_var)

/** Check if a spinlock is held.
 * @param lock		Spinlock to check.
 * @return		True if lock is locked, false otherwise. */
static inline bool spinlock_held(spinlock_t *lock) {
	return atomic_get(&lock->value) != 1;
}

extern status_t spinlock_lock_etc(spinlock_t *lock, useconds_t timeout, int flags);
extern status_t spinlock_lock_ni_etc(spinlock_t *lock, useconds_t timeout, int flags);
extern void spinlock_lock(spinlock_t *lock);
extern void spinlock_lock_ni(spinlock_t *lock);
extern void spinlock_unlock(spinlock_t *lock);
extern void spinlock_unlock_ni(spinlock_t *lock);
extern void spinlock_init(spinlock_t *lock, const char *name);

#endif /* __SYNC_SPINLOCK_H */
