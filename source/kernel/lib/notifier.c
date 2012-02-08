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

/** Initialize a notifier.
 * @param notif		Notifier to initialize.
 * @param data		Pointer to pass as first argument to functions. */
void notifier_init(notifier_t *notif, void *data) {
	mutex_init(&notif->lock, "notifier_lock", 0);
	list_init(&notif->functions);
	notif->data = data;
}

/**
 * Remove all functions from a notifier.
 *
 * Removes all functions registered with a notifier and frees data used to
 * store function information.
 *
 * @param notif		Notifier to destroy.
 */
void notifier_clear(notifier_t *notif) {
	notifier_func_t *nf;

	mutex_lock(&notif->lock);
	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);
		list_remove(&nf->header);
		kfree(nf);
	}
	mutex_unlock(&notif->lock);
}

/**
 * Runs all functions on a notifier.
 *
 * Runs all the functions currently registered for a notifier, without taking
 * the lock.
 *
 * @param notif		Notifier to run.
 * @param data		Pointer to pass as second argument to functions.
 * @param destroy	Whether to remove functions after calling them.
 *
 * @return		Whther any handlers were called.
 */
bool notifier_run_unlocked(notifier_t *notif, void *data, bool destroy) {
	notifier_func_t *nf;
	bool called = false;

	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);
		nf->func(notif->data, data, nf->data);
		if(destroy) {
			list_remove(&nf->header);
			kfree(nf);
		}

		called = true;
	}

	return called;
}

/** Runs all functions on a notifier.
 * @param notif		Notifier to run.
 * @param data		Pointer to pass as second argument to functions.
 * @param destroy	Whether to remove functions after calling them.
 * @return		Whther any handlers were called. */
bool notifier_run(notifier_t *notif, void *data, bool destroy) {
	bool ret;

	mutex_lock(&notif->lock);
	ret = notifier_run_unlocked(notif, data, destroy);
	mutex_unlock(&notif->lock);

	return ret;
}

/** Add a function to a notifier.
 * @param notif		Notifier to add to.
 * @param func		Function to add.
 * @param data		Pointer to pass as third argument to function. */
void notifier_register(notifier_t *notif, void (*func)(void *, void *, void *), void *data) {
	notifier_func_t *nf = kmalloc(sizeof(notifier_func_t), MM_SLEEP);

	list_init(&nf->header);
	nf->func = func;
	nf->data = data;

	mutex_lock(&notif->lock);
	list_append(&notif->functions, &nf->header);
	mutex_unlock(&notif->lock);
}

/** Remove a function from a notifier.
 * @param notif		Notifier to remove from.
 * @param func		Function to remove.
 * @param data		Data argument function was registered with. */
void notifier_unregister(notifier_t *notif, void (*func)(void *, void *, void *), void *data) {
	notifier_func_t *nf;

	mutex_lock(&notif->lock);

	LIST_FOREACH_SAFE(&notif->functions, iter) {
		nf = list_entry(iter, notifier_func_t, header);

		if(nf->func == func && nf->data == data) {
			list_remove(&nf->header);
			kfree(nf);
		}
	}

	mutex_unlock(&notif->lock);
}
