/* Kiwi spinlock implementation
 * Copyright (C) 2008-2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Spinlock implementation.
 */

#ifndef __SYNC_SPINLOCK_H
#define __SYNC_SPINLOCK_H

#include <sync/flags.h>

#include <types/atomic.h>

#include <types.h>

/** Structure containing a spinlock. */
typedef struct spinlock {
	const char *name;		/**< Name of the spinlock. */
	atomic_t locked;		/**< Whether the lock is taken. */
	volatile int state;		/**< Interrupt state prior to locking. */
} spinlock_t;

/** Initialises a statically-declared spinlock. */
#define SPINLOCK_INITIALISER(_name)	\
	{ \
		.name = _name, \
		.locked = 0, \
		.state = 0, \
	}

/** Statically declares a new spinlock. */
#define SPINLOCK_DECLARE(_var)		\
	spinlock_t _var = SPINLOCK_INITIALISER(#_var)

/** Check if a spinlock is held.
 *
 * Checks if the specified spinlock is held.
 *
 * @param lock		Spinlock to check.
 *
 * @return		True if lock is locked, false otherwise.
 */
static inline bool spinlock_held(spinlock_t *lock) {
	return atomic_get(&lock->locked);
}

extern int spinlock_lock(spinlock_t *lock, int flags);
extern int spinlock_lock_ni(spinlock_t *lock, int flags);
extern void spinlock_unlock(spinlock_t *lock);
extern void spinlock_unlock_ni(spinlock_t *lock);
extern void spinlock_init(spinlock_t *lock, const char *name);

#endif /* __SYNC_SPINLOCK_H */
