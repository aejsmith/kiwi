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
 * @brief		Wait queue functions.
 */

#ifndef __SYNC_WAITQ_H
#define __SYNC_WAITQ_H

#include <lib/list.h>

#include <sync/flags.h>
#include <sync/spinlock.h>

struct mutex;

/** Structure containing a thread wait queue. */
typedef struct waitq {
	spinlock_t lock;		/**< Lock to protect the queue. */
	list_t threads;			/**< List of threads on the queue. */
	const char *name;		/**< Name of wait queue. */
} waitq_t;

/** Initialises a statically declared wait queue. */
#define WAITQ_INITIALISER(_var, _name)	\
	{ \
		.lock = SPINLOCK_INITIALISER("waitq_lock"), \
		.threads = LIST_INITIALISER(_var.threads), \
		.name = _name, \
	}

/** Statically declares a new wait queue. */
#define WAITQ_DECLARE(_var)		\
	waitq_t _var = WAITQ_INITIALISER(_var, #_var)

extern bool waitq_sleep_prepare(waitq_t *queue);
extern int waitq_sleep_unsafe(waitq_t *queue, timeout_t timeout, int flags, bool state);
extern int waitq_sleep(waitq_t *queue, timeout_t timeout, int flags);

extern bool waitq_wake_unsafe(waitq_t *queue);
extern bool waitq_wake(waitq_t *queue);
extern bool waitq_wake_all(waitq_t *queue);

extern bool waitq_empty(waitq_t *queue);

extern void waitq_init(waitq_t *queue, const char *name);

#endif /* __SYNC_WAITQ_H */
