/* Kiwi readers-writer lock implementation
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Readers-writer lock implementation.
 */

#ifndef __SYNC_RWLOCK_H
#define __SYNC_RWLOCK_H

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <types/atomic.h>

/** Structure containing a readers-writer lock. */
typedef struct rwlock {
	spinlock_t lock;		/**< Lock to protect structure. */
	semaphore_t exclusive;		/**< Semaphore controlling exclusive access. */
	size_t readers;			/**< Number of readers of the lock. */
} rwlock_t;

/** Initializes a statically declared readers-writer lock. */
#define RWLOCK_INITIALIZER(_var, _name)		\
	{ \
		.lock = SPINLOCK_INITIALIZER("rwlock_lock"), \
		.exclusive = SEMAPHORE_INITIALIZER(_var.exclusive, _name, 1), \
		.readers = 0, \
	}

/** Statically declares a new readers-writer lock. */
#define RWLOCK_DECLARE(_var)			\
	rwlock_t _var = RWLOCK_INITIALIZER(_var, #_var)

extern int rwlock_read_lock(rwlock_t *lock, int flags);
extern int rwlock_write_lock(rwlock_t *lock, int flags);
extern void rwlock_unlock(rwlock_t *lock);

extern void rwlock_init(rwlock_t *lock, const char *name);

#endif /* __SYNC_RWLOCK_H */
