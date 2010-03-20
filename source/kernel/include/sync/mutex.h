/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Mutex implementation.
 */

#ifndef __SYNC_MUTEX_H
#define __SYNC_MUTEX_H

#include <sync/waitq.h>

/** Structure containing a mutex. */
typedef struct mutex {
	atomic_t locked;		/**< Lock count. */
	waitq_t queue;			/**< Queue for threads to wait on. */
	struct thread *holder;		/**< Thread holding the lock. */
	int flags;			/**< Behaviour flags for the mutex. */
} mutex_t;

/** Initialises a statically declared mutex. */
#define MUTEX_INITIALISER(_var, _name, _flags)	\
	{ \
		.locked = 0, \
		.queue = WAITQ_INITIALISER(_var.queue, _name), \
		.holder = NULL, \
		.flags = _flags, \
	}

/** Statically declares a new mutex. */
#define MUTEX_DECLARE(_var, _flags)		\
	mutex_t _var = MUTEX_INITIALISER(_var, #_var, _flags)

/** Mutex behaviour flags. */
#define MUTEX_RECURSIVE		(1<<0)	/**< Allow recursive locking by a thread. */

/** Check if a mutex is held.
 * @param lock		Mutex to check.
 * @return		Whether the mutex is held. */
static inline bool mutex_held(mutex_t *lock) {
	return atomic_get(&lock->locked) != 0;
}

/** Get the current recursion count of a mutex.
 * @param lock		Mutex to check.
 * @return		Current recursion count of mutex. */
static inline int mutex_recursion(mutex_t *lock) {
	return atomic_get(&lock->locked);
}

extern int mutex_lock_etc(mutex_t *lock, useconds_t timeout, int flags);
extern void mutex_lock(mutex_t *lock);
extern void mutex_unlock(mutex_t *lock);
extern void mutex_init(mutex_t *lock, const char *name, int flags);

#endif /* __SYNC_MUTEX_H */
