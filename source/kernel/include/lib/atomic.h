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
 * @brief		x86 atomic operations.
 */

#ifndef __LIB_ATOMIC_H
#define __LIB_ATOMIC_H

#include <types.h>

/** Atomic variable type. */
typedef volatile int atomic_t;

/** Atomic compare-and-set operation.
 *
 * Compares an atomic variable with another value. If they are equal,
 * atomically sets the variable to the specified value.
 *
 * @param var		Pointer to atomic variable.
 * @param cmp		Value to compare with.
 * @param num		Value to set to if equal.
 *
 * @return		True if were equal, false if not.
 */
static inline bool atomic_cmp_set(atomic_t *var, int cmp, int num) {
	return __sync_bool_compare_and_swap(var, cmp, num);
}

/** Atomic add operation.
 *
 * Atomically adds a value to an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 * @param val		Value to add.
 *
 * @return		Old value of variable.
 */
static inline int atomic_add(atomic_t *var, int val) {
	return __sync_fetch_and_add(var, val);
}

/** Atomic subtract operation.
 *
 * Atomically subtracts a value from an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 * @param val		Value to subtract.
 *
 * @return		Old value of variable.
 */
static inline int atomic_sub(atomic_t *var, int val) {
	return __sync_fetch_and_sub(var, val);
} 

/** Atomic increment operation.
 *
 * Atomically increments an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 *
 * @return		Old value of variable.
 */
static inline int atomic_inc(atomic_t *var) {
	return __sync_fetch_and_add(var, 1);
}

/** Atomic decrement operation.
 *
 * Atomically decrements an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 *
 * @return		Old value of variable.
 */
static inline int atomic_dec(atomic_t *var) {
	return __sync_fetch_and_sub(var, 1);
}

/** Atomic set operation.
 *
 * Atomically sets an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 * @param val		Value to set to.
 */
static inline void atomic_set(atomic_t *var, int val) {
	*var = val;
}

/** Atomic get operation.
 *
 * Atomically gets an atomic variable.
 *
 * @param var		Pointer to atomic variable.
 *
 * @return		Value contained in variable.
 */
static inline int atomic_get(atomic_t *var) {
	return *var;
}

#endif /* __LIB_ATOMIC_H */
