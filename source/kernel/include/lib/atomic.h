/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Atomic operations.
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
