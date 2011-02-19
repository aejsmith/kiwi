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
 * @brief		Mutex implementation.
 */

#ifndef __SYNC_MUTEX_H
#define __SYNC_MUTEX_H

#include <sync/waitq.h>

/** Structure containing a mutex. */
typedef struct mutex {
	atomic_t locked;		/**< Lock count. */
	waitq_t queue;			/**< Queue for threads to wait on. */
	int flags;			/**< Behaviour flags for the mutex. */
	struct thread *holder;		/**< Thread holding the lock. */
#if CONFIG_DEBUG
	void *caller;			/**< Return address of lock call. */
#endif
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

extern status_t mutex_lock_etc(mutex_t *lock, useconds_t timeout, int flags);
extern void mutex_lock(mutex_t *lock);
extern void mutex_unlock(mutex_t *lock);
extern void mutex_init(mutex_t *lock, const char *name, int flags);

#endif /* __SYNC_MUTEX_H */
