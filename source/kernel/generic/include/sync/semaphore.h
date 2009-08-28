/* Kiwi semaphore implementation
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
 * @brief		Semaphore implementation.
 */

#ifndef __SYNC_SEMAPHORE_H
#define __SYNC_SEMAPHORE_H

#include <sync/flags.h>
#include <sync/waitq.h>

/** Structure containing a semaphore. */
typedef struct semaphore {
	waitq_t queue;			/**< Wait queue implementing the semaphore. */
} semaphore_t;

/** Initializes a statically declared semaphore. */
#define SEMAPHORE_INITIALIZER(_var, _name, _initial)	\
	{ \
		.queue = WAITQ_INITIALIZER(_var.queue, _name, WAITQ_COUNT_MISSED, _initial), \
	}

/** Statically declares a new semaphore. */
#define SEMAPHORE_DECLARE(_var, _initial)		\
	semaphore_t _var = SEMAPHORE_INITIALIZER(_var, #_var, _initial)

extern int semaphore_down(semaphore_t *sem, int flags);
extern void semaphore_up(semaphore_t *sem);
extern void semaphore_init(semaphore_t *sem, const char *name, unsigned int initial);

#endif /* __SYNC_SEMAPHORE_H */
