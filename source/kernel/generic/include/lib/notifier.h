/* Kiwi event notification system
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Event notification system.
 */

#ifndef __LIB_NOTIFIER_H
#define __LIB_NOTIFIER_H

#include <sync/mutex.h>

#include <types/list.h>

/** Notifier structure. */
typedef struct notifier {
	mutex_t lock;			/**< Lock to protect list. */
	list_t functions;		/**< Functions to call when the event occurs. */
	void *data;			/**< Data to pass to functions. */
} notifier_t;

extern void notifier_init(notifier_t *notif, void *data);
extern void notifier_destroy(notifier_t *notif);
extern void notifier_run(notifier_t *notif, void *data);
extern void notifier_register(notifier_t *notif, void (*func)(void *, void *, void *), void *data);
extern void notifier_unregister(notifier_t *notif, void (*func)(void *, void *, void *), void *data);

#endif /* __LIB_NOTIFIER_H */