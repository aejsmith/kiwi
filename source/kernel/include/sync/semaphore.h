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
 * @brief		Semaphore implementation.
 */

#ifndef __SYNC_SEMAPHORE_H
#define __SYNC_SEMAPHORE_H

#include <sync/flags.h>
#include <sync/waitq.h>

/** Structure containing a semaphore. */
typedef struct semaphore {
	size_t count;			/**< Count of the semaphore. */
	waitq_t queue;			/**< Queue for threads to wait on. */
} semaphore_t;

/** Initialises a statically declared semaphore. */
#define SEMAPHORE_INITIALISER(_var, _name, _initial)	\
	{ \
		.count = _initial, \
		.queue = WAITQ_INITIALISER(_var.queue, _name), \
	}

/** Statically declares a new semaphore. */
#define SEMAPHORE_DECLARE(_var, _initial)		\
	semaphore_t _var = SEMAPHORE_INITIALISER(_var, #_var, _initial)

/** Get the current value of a semaphore.
 * @param sem		Semaphore to get count of.
 * @return		Semaphore's value. */
static inline size_t semaphore_count(semaphore_t *sem) {
	return sem->count;
}

extern int semaphore_down_etc(semaphore_t *sem, useconds_t timeout, int flags);
extern void semaphore_down(semaphore_t *sem);
extern void semaphore_up(semaphore_t *sem, size_t count);
extern void semaphore_init(semaphore_t *sem, const char *name, size_t initial);

#endif /* __SYNC_SEMAPHORE_H */
