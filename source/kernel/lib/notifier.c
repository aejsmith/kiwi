/*
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

#include <mm/malloc.h>

#include <lib/notifier.h>

/** Structure defining a callback function on a notifier. */
typedef struct notifier_func {
	list_t header;				/**< Link to notifier. */

	void (*func)(void *, void *, void *);	/**< Function to call. */
	void *data;				/**< Third data argument for function. */
} notifier_func_t;

/** Initialise a notifier.
 *
 * Initialises a notifier structure.
 *
 * @param notif		Notifier to initialise.
 * @param data		Pointer to pass as first argument to functions.
 */
void notifier_init(notifier_t *notif, void *data) {
	mutex_init(&notif->lock, "notifier_lock", 0);
	list_init(&notif->functions);
	notif->data = data;
}

/** Remove all functions from a notifier.
 *
 * Removes all functions registered with a notifier and frees data used to
 * store function information.
 *
 * @param notif		Notifier to destroy.
 */
void notifier_empty(notifier_t *notif) {
	notifier_func_t *nf;

	mutex_lock(&notif->lock, 0);
	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);

		list_remove(&nf->header);
		kfree(nf);
	}
	mutex_unlock(&notif->lock);
}

/** Runs all functions on a notifier.
 *
 * Runs all the functions currently registered for a notifier, without taking
 * the lock.
 *
 * @param notif		Notifier to run.
 * @param data		Pointer to pass as second argument to functions.
 * @param destroy	Whether to remove functions after calling them.
 */
void notifier_run_unlocked(notifier_t *notif, void *data, bool destroy) {
	notifier_func_t *nf;

	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);
		nf->func(notif->data, data, nf->data);
		if(destroy) {
			list_remove(&nf->header);
			kfree(nf);
		}
	}
}

/** Runs all functions on a notifier.
 *
 * Runs all the functions currently registered for a notifier.
 *
 * @param notif		Notifier to run.
 * @param data		Pointer to pass as second argument to functions.
 * @param destroy	Whether to remove functions after calling them.
 */
void notifier_run(notifier_t *notif, void *data, bool destroy) {
	mutex_lock(&notif->lock, 0);
	notifier_run_unlocked(notif, data, destroy);
	mutex_unlock(&notif->lock);
}

/** Add a function to a notifier.
 *
 * Register a function to be called when a notifier is run.
 *
 * @param notif		Notifier to add to.
 * @param func		Function to add.
 * @param data		Pointer to pass as third argument to function.
 */
void notifier_register(notifier_t *notif, void (*func)(void *, void *, void *), void *data) {
	notifier_func_t *nf = kmalloc(sizeof(notifier_func_t), MM_SLEEP);

	list_init(&nf->header);
	nf->func = func;
	nf->data = data;

	mutex_lock(&notif->lock, 0);
	list_append(&notif->functions, &nf->header);
	mutex_unlock(&notif->lock);
}

/** Remove a function from a notifier.
 *
 * Removes any functions matching the specified arguments from a notifier.
 *
 * @param notif		Notifier to remove from.
 * @param func		Function to remove.
 * @param data		Data argument function was registered with.
 */
void notifier_unregister(notifier_t *notif, void (*func)(void *, void *, void *), void *data) {
	notifier_func_t *nf;

	mutex_lock(&notif->lock, 0);

	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);

		if(nf->func == func && nf->data == data) {
			list_remove(&nf->header);
			kfree(nf);
		}
	}

	mutex_unlock(&notif->lock);
}
