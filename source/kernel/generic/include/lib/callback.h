/* Kiwi event callback system
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
 * @brief		Event callback mechanism.
 *
 * The interface defined in this header allows higher-level layers within the
 * kernel (i.e. modules) to register functions with the core kernel (or
 * lower-level modules) to be called when certain events occur.
 *
 * Idea from Linux's notifier system.
 */

#ifndef __LIB_CALLBACK_H
#define __LIB_CALLBACK_H

#include <types/list.h>

/** Callback list structure. */
typedef list_t callback_list_t;

/** Structure defining a callback on a callback list. */
typedef struct callback {
	list_t header;			/**< List header. */
	void (*func)(void *, void *);	/**< Actual callback function. */
	void *data;			/**< Data passed as second argument to callback. */
} callback_t;

/** Initializer for a statically-declared callback. */
#define CALLBACK_INITIALIZER(_name, _func, _data)	\
	{ \
		.header = LIST_INITIALIZER(_name.header), \
		.data = _data, \
		.func = _func \
	}

/** Statically declares a new callback. */
#define CALLBACK_DECLARE(_name, _func, _data)		\
	callback_t _name = CALLBACK_INITIALIZER(_name, _func, _data)

/** Initializer for a statically-declared callback list. */
#define CALLBACK_LIST_INITIALIZER(_name)	LIST_INITIALIZER(_name)

/** Statically declares a new callback list. */
#define CALLBACK_LIST_DECLARE(_name)	\
	callback_list_t _name = CALLBACK_LIST_INITIALIZER(_name)

/** Initialize a callback list.
 *
 * Initializes the given callback list structure.
 *
 * @param list		List to initialize.
 */
static inline void callback_list_init(callback_list_t *list) {
	list_init(list);
}

/** Runs all callbacks on a callback list.
 *
 * Runs all the callbacks currently on a callback list. The given data
 * argument is passed as the first argument to each function, and the data
 * for each callback is passed as the second.
 *
 * @param list		List to run.
 * @param data		Argument to pass to each callback function.
 */
static inline void callback_list_run(callback_list_t *list, void *data) {
	callback_t *cb;

	LIST_FOREACH_SAFE(list, iter) {
		cb = list_entry(iter, callback_t, header);
		cb->func(data, cb->data);
	}
}

/** Add a callback to a callback list.
 *
 * Adds a callback structure to a callback list.
 *
 * @param list		List to add to.
 * @param cb		Callback structure to add.
 */
static inline void callback_add(callback_list_t *list, callback_t *cb) {
	list_append(list, &cb->header);
}

/** Remove a callback from the list it is in.
 *
 * Removes a callback structure from whatever list currently contains it.
 *
 * @param cb		Callback to remove.
 */
static inline void callback_remove(callback_t *cb) {
	list_remove(&cb->header);
}

/** Initialize a callback structure.
 *
 * Initializes a callback structure.
 *
 * @param cb		Callback to initialize.
 * @param func		Function for the callback.
 * @param data		Data to pass as second function argument.
 */
static inline void callback_init(callback_t *cb, void (*func)(void *, void *), void *data) {
	/* Not strictly necessary, callback_add() does this too. */
	list_init(&cb->header);

	cb->data = data;
	cb->func = func;
}

#endif /* __LIB_CALLBACK_H */
