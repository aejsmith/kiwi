/* Kiwi RTLD utility functions
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
 * @brief		RTLD utility functions.
 *
 * The RTLD implements a small set of functions from the C library that are
 * needed by it. If a function that's in the C library is required, a copy of
 * it should be placed in utility.c. However, in order to keep the code small,
 * this should be avoided where possible. 
 */

#ifndef __RTLD_UTILITY_H
#define __RTLD_UTILITY_H

#include <alloca.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define __need_offsetof
#include <stddef.h>

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

extern void dprintf(const char *format, ...);

#endif /* __RTLD_UTILITY_H */
