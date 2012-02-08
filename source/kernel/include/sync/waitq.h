/*
 * Copyright (C) 2008-2011 Alex Smith
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
		.threads = LIST_INITIALIZER(_var.threads), \
		.name = _name, \
	}

/** Statically declares a new wait queue. */
#define WAITQ_DECLARE(_var)		\
	waitq_t _var = WAITQ_INITIALISER(_var, #_var)

extern bool waitq_sleep_prepare(waitq_t *queue);
extern void waitq_sleep_cancel(waitq_t *queue, bool state);
extern status_t waitq_sleep_unsafe(waitq_t *queue, useconds_t timeout, int flags, bool state);
extern status_t waitq_sleep(waitq_t *queue, useconds_t timeout, int flags);

extern bool waitq_wake_unsafe(waitq_t *queue);
extern bool waitq_wake(waitq_t *queue);
extern bool waitq_wake_all(waitq_t *queue);

extern bool waitq_empty(waitq_t *queue);

extern void waitq_init(waitq_t *queue, const char *name);

#endif /* __SYNC_WAITQ_H */
