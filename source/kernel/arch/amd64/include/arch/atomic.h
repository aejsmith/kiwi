/*
 * Copyright (C) 2009-2020 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief               AMD64 atomic operations.
 */

#pragma once

#include <types.h>

/** Atomic variable type (32-bit). */
typedef volatile int32_t atomic_t;

/** Retrieve the value of an atomic variable.
 * @param var           Pointer to atomic variable.
 * @return              Value contained in variable. */
static inline int32_t atomic_get(const atomic_t *var) {
    return *var;
}

/** Set the value of an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to set to. */
static inline void atomic_set(atomic_t *var, int32_t val) {
    *var = val;
}

/** Atomically add a value to an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to add.
 * @return              Previous value of variable. */
static inline int32_t atomic_add(atomic_t *var, int32_t val) {
    __asm__ volatile(
        "lock xaddl %0, %1"
        : "+r"(val), "+m"(*var)
        :: "memory");

    return val;
}

/** Atomically subtract a value from an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to subtract.
 * @return              Previous value of variable. */
static inline int32_t atomic_sub(atomic_t *var, int32_t val) {
    return atomic_add(var, -val);
}

/** Swap the value of an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to set to.
 * @return              Previous value of the variable. */
static inline int32_t atomic_swap(atomic_t *var, int32_t val) {
    __asm__ volatile(
        "lock xchgl %0, %1"
        : "+r"(val), "+m"(*var)
        :: "memory");

    return val;
}

/**
 * Perform an atomic compare-and-set operation.
 *
 * Compares an atomic variable with another value. If they are equal, sets
 * the variable to the specified value. The whole operation is atomic.
 *
 * @param var           Pointer to atomic variable.
 * @param cmp           Value to compare with.
 * @param val           Value to set to if equal.
 *
 * @return              Previous value of variable. If this is equal to cmp,
 *                      the operation succeeded.
 */
static inline int32_t atomic_cas(atomic_t *var, int32_t cmp, int32_t val) {
    int32_t r;

    __asm__ volatile(
        "lock cmpxchgl  %3, %1\n\t"
        : "=a"(r), "+m"(*var)
        : "0"(cmp), "r"(val));

    return r;
}

/** Atomic variable type (64-bit). */
typedef volatile int64_t atomic64_t;

/** Retrieve the value of an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @return              Value contained in variable. */
static inline int64_t atomic_get64(atomic64_t *var) {
    return *var;
}

/** Set the value of an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @param val           Value to set to. */
static inline void atomic_set64(atomic64_t *var, int64_t val) {
    *var = val;
}

/** Atomically add a value to an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @param val           Value to add.
 * @return              Previous value of variable. */
static inline int64_t atomic_add64(atomic64_t *var, int64_t val) {
    __asm__ volatile(
        "lock xaddq %0, %1"
        : "+r"(val), "+m"(*var)
        :: "memory");

    return val;
}

/** Atomically subtract a value from an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @param val           Value to subtract.
 * @return              Previous value of variable. */
static inline int64_t atomic_sub64(atomic64_t *var, int64_t val) {
    return atomic_add64(var, -val);
}

/** Swap the value of an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to set to.
 * @return              Previous value of the variable. */
static inline int64_t atomic_swap64(atomic64_t *var, int64_t val) {
    __asm__ volatile(
        "lock xchgq %0, %1"
        : "+r"(val), "+m"(*var)
        :: "memory");

    return val;
}

/**
 * Perform an atomic compare-and-set operation (64-bit).
 *
 * Compares an atomic variable with another value. If they are equal, sets
 * the variable to the specified value. The whole operation is atomic.
 *
 * @param var           Pointer to atomic variable.
 * @param cmp           Value to compare with.
 * @param val           Value to set to if equal.
 *
 * @return              Previous value of variable. If this is equal to cmp,
 *                      the operation succeeded.
 */
static inline int64_t atomic_cas64(atomic64_t *var, int64_t cmp, int64_t val) {
    int64_t r;

    __asm__ volatile(
        "lock\n\t"
        "cmpxchgq   %3, %1"
        : "=a"(r), "+m"(*var)
        : "0"(cmp), "r"(val));

    return r;
}
