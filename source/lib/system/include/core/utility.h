/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Utility functions.
 */

#pragma once

#include <assert.h>
#include <stddef.h>

/** Compiler attribute/builtin macros. */
#define core_likely(x)          __builtin_expect(!!(x), 1)
#define core_unlikely(x)        __builtin_expect(!!(x), 0)

#ifdef NDEBUG
#   define core_unreachable()   __builtin_unreachable()
#else
#   define core_unreachable()   assert(false)
#endif

/** Get the number of elements in an array. */
#define core_array_size(a)  (sizeof((a)) / sizeof((a)[0]))

/** Round a value up.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define core_round_up(val, nearest) \
    __extension__ \
    ({ \
        __typeof__(val) __n = val; \
        if (__n % (nearest)) { \
            __n -= __n % (nearest); \
            __n += nearest; \
        } \
        __n; \
    })

/** Round a value down.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define core_round_down(val, nearest) \
    __extension__ \
    ({ \
        __typeof__(val) __n = val; \
        if (__n % (nearest)) \
            __n -= __n % (nearest); \
        __n; \
    })

/** Check if a value is a power of 2.
 * @param val           Value to check.
 * @return              Whether value is a power of 2. */
#define core_is_pow2(val) \
    ((val) && ((val) & ((val) - 1)) == 0)

/** Get the lowest value out of a pair of values. */
#define core_min(a, b) \
    ((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define core_max(a, b) \
    ((a) < (b) ? (b) : (a))

/** Swap two values. */
#define core_swap(a, b) \
    { \
        __typeof__(a) __tmp = a; \
        a = b; \
        b = __tmp; \
    }

/** Get a pointer to the object containing a given object.
 * @param ptr           Pointer to child object.
 * @param type          Type of parent object.
 * @param member        Member in parent.
 * @return              Pointer to parent object. */
#define core_container_of(ptr, type, member) \
    __extension__ \
    ({ \
        const __typeof__(((type *)0)->member) *__mptr = ptr; \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })
