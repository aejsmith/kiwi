/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Utility functions/macros.
 */

#pragma once

#include <types.h>

/** Get the number of bits in a type. */
#define type_bits(t)        (sizeof(t) * 8)

/** Get the number of elements in an array. */
#define array_size(a)       (sizeof((a)) / sizeof((a)[0]))

/** Round a value up.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define round_up(val, nearest) \
    __extension__ \
    ({ \
        typeof(val) __n = val; \
        if (__n % (nearest)) { \
            __n -= __n % (nearest); \
            __n += nearest; \
        } \
        __n; \
    })

/**
 * Round a value up to a power of 2.
 *
 * Rounds a value up to a power of 2. Note that when the round_up() macro is
 * used with a constant the compiler will most likely optimise that itself.
 * This is useful for rounding to a variable which is known to be a power of 2.
 *
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 *
 * @return              Rounded value.
 */
#define round_up_pow2(val, nearest) \
    __extension__ \
    ({ \
        typeof(val) __n = val; \
        if (__n & ((nearest) - 1)) { \
            __n -= __n & ((nearest) - 1); \
            __n += nearest; \
        } \
        __n; \
    })

/** Round a value down.
 * @param val           Value to round.
 * @param nearest       Boundary to round up to.
 * @return              Rounded value. */
#define round_down(val, nearest) \
    __extension__ \
    ({ \
        typeof(val) __n = val; \
        if (__n % (nearest)) \
            __n -= __n % (nearest); \
        __n; \
    })

/** Check if a value is a power of 2.
 * @param val           Value to check.
 * @return              Whether value is a power of 2. */
#define is_pow2(val) \
    ((val) && ((val) & ((val) - 1)) == 0)

/** Get the lowest value out of a pair of values. */
#define min(a, b) \
    ((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define max(a, b) \
    ((a) < (b) ? (b) : (a))

/** Swap two values. */
#define swap(a, b) \
    { \
        typeof(a) __tmp = a; \
        a = b; \
        b = __tmp; \
    }

/** Get a pointer to the object containing a given object.
 * @param ptr           Pointer to child object.
 * @param type          Type of parent object.
 * @param member        Member in parent.
 * @return              Pointer to parent object. */
#define container_of(ptr, type, member) \
    __extension__ \
    ({ \
        const typeof(((type *)0)->member) *__mptr = ptr; \
        (type *)((char *)__mptr - offsetof(type, member)); \
    })

/**
 * Define an inline helper function to cast a base "class" structure pointer
 * to a derived "class". This is used where a derived class structure has its
 * base class structure embedded inside it as a member.
 *
 * For example, given the following:
 *
 *     typedef struct my_base {
 *         ...
 *     } my_base_t;
 *
 *     typedef struct my_derived {
 *         ...
 *         my_base_t base;
 *         ...
 *     } my_derived_t;
 *
 *     DEFINE_CLASS_CAST(my_derived, my_base, base);
 *
 * You can then do:
 *
 *     void my_derived_func(my_base_t *_obj) {
 *          my_derived_t *obj = cast_my_derived(_obj);
 *          ...
 *     }
 *
 * The base class structure can be anywhere within the derived structure, since
 * container_of is used to offset the pointer correctly.
 *
 * @param type          Derived class structure name. The struct name is used
 *                      rather than the _t typedef here, so that the _t part can
 *                      be omitted from the function name.
 * @param base          Base class structure name. Again, struct name rather
 *                      than the _t typedef.
 * @param member        Name of base class member within derived.
 */
#define DEFINE_CLASS_CAST(type, base, member) \
    static inline struct type *cast_##type(struct base *p) { \
        return container_of(p, struct type, member); \
    }

/** Find first set bit in a native-sized value.
 * @param value         Value to test.
 * @return              Position of first set bit plus 1, or 0 if value is 0. */
static inline unsigned long ffs(unsigned long value) {
    return __builtin_ffsl(value);
}

/** Find first zero bit in a native-sized value.
 * @param value         Value to test.
 * @return              Position of first zero bit plus 1, or 0 if all bits are
 *                      set. */
static inline unsigned long ffz(unsigned long value) {
    return __builtin_ffsl(~value);
}

/** Find last set bit in a native-sized value.
 * @param value     Value to test.
 * @return          Position of last set bit plus 1, or 0 if value is 0. */
static inline unsigned long fls(unsigned long value) {
    if (!value)
        return 0;

    return type_bits(unsigned long) - __builtin_clzl(value);
}

#if CONFIG_32BIT

/** Implementation for long long values on 32-bit systems. */
static inline int highbit_ll(unsigned long long val) {
    unsigned long high, low;

    if (!val)
        return 0;

    high = (unsigned long)((val >> 32) & 0xffffffff);
    low = (unsigned long)(val & 0xffffffff);
    if (high) {
        return fls(high) + 32;
    } else {
        return fls(low);
    }
}

#endif /* CONFIG_32BIT */

/** Get log base 2 (high bit) of a value.
 * @param val           Value to get high bit from.
 * @return              High bit + 1. */
#if CONFIG_32BIT
#   define highbit(val)   _Generic((val), \
        unsigned long long: highbit_ll, \
        long long: highbit_ll, \
        default: fls)(val)
#else
#   define highbit(val)   fls(val)
#endif

/** Checksum a memory range.
 * @param start         Start of range to check.
 * @param size          Size of range to check.
 * @return              True if checksum is equal to 0, false if not. */
static inline bool checksum_range(void *start, size_t size) {
    uint8_t *range = (uint8_t *)start;
    uint8_t checksum = 0;
    size_t i;

    for (i = 0; i < size; i++)
        checksum += range[i];

    return (checksum == 0);
}

extern void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *));
