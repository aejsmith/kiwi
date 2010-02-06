/* Kiwi utility functions
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		Utility functions/macros.
 */

#ifndef __LIB_UTILITY_H
#define __LIB_UTILITY_H

#include <arch/bitops.h>

#include <types.h>

/** Round a value up. */
#define ROUND_UP(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
			__n += nearest; \
		} \
		__n; \
	})

/** Round a value down. */
#define ROUND_DOWN(value, nearest) \
	__extension__ \
	({ \
		typeof(value) __n = value; \
		if(__n % (nearest)) { \
			__n -= __n % (nearest); \
		} \
		__n; \
	})

/** Get the number of bits in a type. */
#define BITS(t)		(sizeof(t) * 8)

/** Get the number of elements in an array. */
#define ARRAYSZ(a)	(sizeof((a)) / sizeof((a)[0]))

/** Get the lowest value out of a pair of values. */
#define MIN(a, b)	((a) < (b) ? (a) : (b))

/** Get the highest value out of a pair of values. */
#define MAX(a, b)	((a) < (b) ? (b) : (a))

/** Get log base 2 (high bit) of a value.
 * @param val		Value to get high bit from.
 * @return		High bit + 1. */
static inline int highbit(uint64_t val) {
	if(!val) {
		return 0;
	}

#if CONFIG_ARCH_64BIT
	return bitops_fls(val) + 1;
#elif CONFIG_ARCH_32BIT
	unative_t high, low;

	high = (unative_t)((val >> 32) & 0xffffffff);
	low = (unative_t)(val & 0xffffffff);
	if(high) {
		return bitops_fls(high) + 32 + 1;
	} else {
		return bitops_fls(low) + 1;
	}
#else
# error "Implement this."
#endif
}

#endif /* __LIB_UTILITY_H */