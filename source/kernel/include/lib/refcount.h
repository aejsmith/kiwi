/*
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
 * @brief		Reference counting functions.
 */

#ifndef __LIB_REFCOUNT_H
#define __LIB_REFCOUNT_H

#include <lib/atomic.h>
#include <fatal.h>

/** Type containing a reference count */
typedef atomic_t refcount_t;

/** Initialises a statically declared reference count. */
#define REFCOUNT_INITIALISER(_initial)		_initial

/** Statically declares a new reference count. */
#define REFCOUNT_DECLARE(_var, _initial)	\
	refcount_t _var = REFCOUNT_INITIALISER(_initial)

/** Increase a reference count.
 *
 * Atomically increases the value of a reference count.
 *
 * @param ref		Reference count to increase.
 *
 * @return		The new value of the count.
 */
static inline int refcount_inc(refcount_t *ref) {
	return atomic_inc(ref) + 1;
}

/** Decrease a reference count.
 *
 * Atomically decreases the value of a reference count. If it goes below 0
 * then a fatal() call will be made.
 *
 * @param ref		Reference count to decrease.
 *
 * @return		The new value of the count.
 */
static inline int refcount_dec(refcount_t *ref) {
	int val = atomic_dec(ref) - 1;

	if(unlikely(val < 0)) {
		fatal("Reference count %p went negative", ref);
	}

	return val;
}

/** Decrease a reference count.
 *
 * Atomically decreases the value of a reference count. If it goes below 0
 * then the specified function will be called with a pointer to the reference
 * count as a parameter.
 *
 * @param ref		Reference count to decrease.
 * @param func		Function to call if count goes negative.
 *
 * @return		The new value of the count.
 */
static inline int refcount_dec_func(refcount_t *ref, void (*func)(refcount_t *)) {
	int val = atomic_dec(ref) - 1;

	if(unlikely(val < 0)) {
		func(ref);
	}

	return val;
}

/** Get the value of a reference count.
 *
 * Atomically gets the current value of a reference count.
 *
 * @param ref		Reference count to get value of.
 *
 * @return		The value of the count.
 */
static inline int refcount_get(refcount_t *ref) {
	return atomic_get(ref);
}

/** Set the value of a reference count.
 *
 * Atomically sets the current value of a reference count to the given value.
 *
 * @param ref		Reference count to set.
 * @param val		Value to set to.
 */
static inline void refcount_set(refcount_t *ref, int val) {
	atomic_set(ref, val);
}

#endif /* __LIB_REFCOUNT_H */
