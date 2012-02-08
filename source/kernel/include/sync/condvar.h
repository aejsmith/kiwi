/*
 * Copyright (C) 2009-2010 Alex Smith
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

/** Initializes a statically declared condition variable. */
#define CONDVAR_INITIALIZER(_var, _name)	\
	{ \
		.queue = WAITQ_INITIALIZER(_var.queue, _name), \
	}

/** Statically declares a new condition variable. */
#define CONDVAR_DECLARE(_var)			\
	condvar_t _var = CONDVAR_INITIALIZER(_var, #_var)

extern status_t condvar_wait_etc(condvar_t *cv, mutex_t *mtx, spinlock_t *sl, useconds_t timeout, int flags);
extern void condvar_wait(condvar_t *cv, mutex_t *mtx, spinlock_t *sl);
extern bool condvar_signal(condvar_t *cv);
extern bool condvar_broadcast(condvar_t *cv);

extern void condvar_init(condvar_t *cv, const char *name);

#endif /* __SYNC_CONDVAR_H */
