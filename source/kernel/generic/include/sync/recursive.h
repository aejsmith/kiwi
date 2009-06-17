/* Kiwi recursive lock implementation
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
 * @brief		Recursive lock implementation.
 */

#ifndef __SYNC_RECURSIVE_H
#define __SYNC_RECURSIVE_H

#include <sync/semaphore.h>

/** Structure containing a recursive lock. */
typedef struct recursive_lock {
	semaphore_t sem;		/**< Queue of waiting threads. */
	struct thread *holder;		/**< Thread holding the lock. */
	int recursion;			/**< Recursion count. */
} recursive_lock_t;

/** Initializes a statically declared recursive lock. */
#define RECURSIVE_LOCK_INITIALIZER(_var, _name)		\
	{ \
		.queue = WAITQ_INITIALIZER(_var.queue, _name, 0, 0), \
		.locked = 0, \
		.holder = NULL, \
	}

/** Statically declares a new recursive lock. */
#define RECURSIVE_LOCK_DECLARE(_var)			\
	recursive_lock_t _var = RECURSIVE_LOCK_INITIALIZER(_var, #_var)

extern int recursive_lock_acquire(recursive_lock_t *lock, int flags);
extern void recursive_lock_release(recursive_lock_t *lock);
extern void recursive_lock_init(recursive_lock_t *lock, const char *name);

#endif /* __SYNC_RECURSIVE_H */
