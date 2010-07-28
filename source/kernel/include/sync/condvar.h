/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Condition variable code.
 */

#ifndef __SYNC_CONDVAR_H
#define __SYNC_CONDVAR_H

#include <sync/mutex.h>
#include <sync/waitq.h>

/** Structure containing a condition variable. */
typedef struct condvar {
	waitq_t queue;			/**< Wait queue implementing the condition variable. */
} condvar_t;

/** Initialises a statically declared condition variable. */
#define CONDVAR_INITIALISER(_var, _name)	\
	{ \
		.queue = WAITQ_INITIALISER(_var.queue, _name), \
	}

/** Statically declares a new condition variable. */
#define CONDVAR_DECLARE(_var)			\
	condvar_t _var = CONDVAR_INITIALISER(_var, #_var)

extern status_t condvar_wait_etc(condvar_t *cv, mutex_t *mtx, spinlock_t *sl, useconds_t timeout, int flags);
extern void condvar_wait(condvar_t *cv, mutex_t *mtx, spinlock_t *sl);
extern bool condvar_signal(condvar_t *cv);
extern bool condvar_broadcast(condvar_t *cv);

extern void condvar_init(condvar_t *cv, const char *name);

#endif /* __SYNC_CONDVAR_H */
