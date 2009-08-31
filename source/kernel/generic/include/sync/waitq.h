/* Kiwi wait queue functions
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
 * @brief		Wait queue functions.
 */

#ifndef __SYNC_WAITQ_H
#define __SYNC_WAITQ_H

#include <sync/flags.h>
#include <sync/spinlock.h>

#include <types/list.h>

struct mutex;
struct thread;

/** Structure containing a thread wait queue. */
typedef struct waitq {
	spinlock_t lock;		/**< Lock to protect the queue. */
	list_t threads;			/**< List of threads on the queue. */
	int flags;			/**< Flags for the wait queue. */
	unsigned int missed;		/**< Number of missed wakeups. */
	const char *name;		/**< Name of wait queue. */
} waitq_t;

/** Initializes a statically declared wait queue. */
#define WAITQ_INITIALIZER(_var, _name, _flags, _missed)	\
	{ \
		.lock = SPINLOCK_INITIALIZER("waitq_lock"), \
		.threads = LIST_INITIALIZER(_var.threads), \
		.flags = _flags, \
		.missed = _missed, \
		.name = _name, \
	}

/** Statically declares a new wait queue. */
#define WAITQ_DECLARE(_var, _flags, _missed)		\
	waitq_t _var = WAITQ_INITIALIZER(_var, #_var, _flags, _missed)

/** Wait queue behaviour flags. */
#define WAITQ_COUNT_MISSED	(1<<0)	/**< Count missed wakeups. */

extern int waitq_sleep(waitq_t *waitq, struct mutex *mtx, spinlock_t *sl, int flags);
extern bool waitq_wake(waitq_t *waitq, bool all);
extern void waitq_interrupt(struct thread *thread);

extern bool waitq_empty(waitq_t *waitq);

extern void waitq_init(waitq_t *waitq, const char *name, int flags);

#endif /* __SYNC_WAITQ_H */
