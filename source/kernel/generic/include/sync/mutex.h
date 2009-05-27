/* Kiwi mutex implementation
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
 * @brief		Mutex implementation.
 */

#ifndef __SYNC_MUTEX_H
#define __SYNC_MUTEX_H

#include <sync/waitq.h>

/** Structure containing a mutex. */
typedef struct mutex {
	wait_queue_t queue;		/**< Queue of waiting threads. */
	atomic_t locked;		/**< Whether the lock is held. */
	struct thread *holder;		/**< Thread holding the lock. */
} mutex_t;

/** Initializes a statically declared mutex. */
#define MUTEX_INITIALIZER(_var, _name)		\
	{ \
		.queue = WAITQ_INITIALIZER(_var.queue, _name, 0, 0), \
		.locked = 0, \
		.holder = NULL, \
	}

/** Statically declares a new mutex. */
#define MUTEX_DECLARE(_var)			\
	mutex_t _var = MUTEX_INITIALIZER(_var, #_var)

/** Check whether a mutex is held.
 * @param mutex		Mutex to check.
 * @return		Whether the mutex is held. */
static inline bool mutex_held(mutex_t *mutex) {
	return atomic_get(&mutex->locked);
}

extern int mutex_lock(mutex_t *mutex, int flags);
extern void mutex_unlock(mutex_t *mutex);
extern void mutex_init(mutex_t *mutex, const char *name);

#endif /* __SYNC_MUTEX_H */
