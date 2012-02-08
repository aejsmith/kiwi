/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Event notification system.
 */

#ifndef __LIB_NOTIFIER_H
#define __LIB_NOTIFIER_H

#include <sync/mutex.h>

/** Notifier structure. */
typedef struct notifier {
	mutex_t lock;			/**< Lock to protect list. */
	list_t functions;		/**< Functions to call when the event occurs. */
	void *data;			/**< Data to pass to functions. */
} notifier_t;

/** Initialises a statically declared notifier. */
#define NOTIFIER_INITIALISER(_var, _data)	\
	{ \
		.lock = MUTEX_INITIALISER(_var.lock, "notifier_lock", 0), \
		.functions = LIST_INITIALIZER(_var.functions), \
		.data = _data, \
	}

/** Statically declares a new notifier. */
#define NOTIFIER_DECLARE(_var, _data)		\
	notifier_t _var = NOTIFIER_INITIALISER(_var, _data)

/** Check if a notifier's function list is empty.
 * @param notif		Notifier to check.
 * @return		Whether the function list is empty. */
static inline bool notifier_empty(notifier_t *notif) {
	return list_empty(&notif->functions);
}

extern void notifier_init(notifier_t *notif, void *data);
extern void notifier_clear(notifier_t *notif);
extern bool notifier_run_unlocked(notifier_t *notif, void *data, bool destroy);
extern bool notifier_run(notifier_t *notif, void *data, bool destroy);
extern void notifier_register(notifier_t *notif, void (*func)(void *, void *, void *), void *data);
extern void notifier_unregister(notifier_t *notif, void (*func)(void *, void *, void *), void *data);

#endif /* __LIB_NOTIFIER_H */
