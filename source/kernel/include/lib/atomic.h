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
 * @brief               Atomic operations.
 */

#pragma once

#include <arch/atomic.h>

/** Atomically increment an atomic variable.
 * @param var           Pointer to atomic variable.
 * @return              Previous value of variable. */
static inline int32_t atomic_inc(atomic_t *var) {
    return atomic_add(var, 1);
}

/** Atomically decrement an atomic variable.
 * @param var           Pointer to atomic variable.
 * @return              Previous value of variable. */
static inline int32_t atomic_dec(atomic_t *var) {
    return atomic_sub(var, 1);
}

/** Atomically OR a value with an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to OR.
 * @return              Previous value of variable. */
static inline int32_t atomic_or(atomic_t *var, int32_t val) {
    int32_t old, new;

    do {
        old = atomic_get(var);
        new = old | val;
    } while (atomic_cas(var, old, new) != old);

    return old;
}

/** Atomically AND a value with an atomic variable.
 * @param var           Pointer to atomic variable.
 * @param val           Value to AND.
 * @return              Previous value of variable. */
static inline int32_t atomic_and(atomic_t *var, int32_t val) {
    int32_t old, new;

    do {
        old = atomic_get(var);
        new = old & val;
    } while (atomic_cas(var, old, new) != old);

    return old;
}

#if CONFIG_64BIT

/** Atomically increment an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @return              Previous value of variable. */
static inline int64_t atomic_inc64(atomic64_t *var) {
    return atomic_add64(var, 1);
}

/** Atomically decrement an atomic variable (64-bit).
 * @param var           Pointer to atomic variable.
 * @return              Previous value of variable. */
static inline int64_t atomic_dec64(atomic64_t *var) {
    return atomic_sub64(var, 1);
}

#endif /* CONFIG_64BIT */
