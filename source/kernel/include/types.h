/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		Type definitions.
 */

#ifndef __TYPES_H
#define __TYPES_H

#include <arch/types.h>

#include <compiler.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/** Internal kernel integer types. */
typedef uint64_t key_t;			/**< Type used to store a key for a container. */

/** Minimum and maximum values a signed char can hold. */
#define SCHAR_MIN	(-SCHAR_MAX - 1)
#define SCHAR_MAX	__SCHAR_MAX__

/** Maximum value an unsigned char can hold. */
#define UCHAR_MAX	((SCHAR_MAX * 2) + 1)

/** Minimum and maximum values a char can hold. */
#ifdef __CHAR_UNSIGNED__
# define CHAR_MIN	0
# define CHAR_MAX	UCHAR_MAX
#else
# define CHAR_MIN	SCHAR_MIN
# define CHAR_MAX	SCHAR_MAX
#endif

/** Minimum and maximum values a signed short can hold. */
#define SHRT_MIN	(-SHRT_MAX - 1)
#define SHRT_MAX	__SHRT_MAX__

/** Maximum value an unsigned short can hold. */
#define USHRT_MAX	((SHRT_MAX * 2) + 1)

/** Minimum and maximum values a signed int can hold. */
#define INT_MIN		(-INT_MAX - 1)
#define INT_MAX		__INT_MAX__

/** Maximum value an unsigned int can hold. */
#define UINT_MAX	((INT_MAX * 2U) + 1U)

/** Minimum and maximum values a signed long can hold. */
#define LONG_MIN	(-LONG_MAX - 1L)
#define LONG_MAX	__LONG_MAX__

/** Maximum value an unsigned long can hold. */
#define ULONG_MAX	((LONG_MAX * 2UL) + 1UL)

/** Minimum and maximum values a signed long long can hold. */
#define LLONG_MIN	(-LLONG_MAX - 1LL)
#define LLONG_MAX	__LONG_LONG_MAX__

/** Maximum value an unsigned long long can hold. */
#define ULLONG_MAX	((LLONG_MAX * 2ULL) + 1ULL)

/** Limits for specific integer types. */
#define INT8_MIN	(-128)
#define INT8_MAX	127
#define UINT8_MAX	255
#define INT16_MIN	(-32767-1)
#define INT16_MAX	32767
#define UINT16_MAX	65535u
#define INT32_MIN	(-2147483647-1)
#define INT32_MAX	2147483647
#define UINT32_MAX	4294967295u
#define INT64_MIN	(-9223372036854775807ll-1)
#define INT64_MAX	9223372036854775807ll
#define UINT64_MAX	18446744073709551615ull

#include <kernel/types.h>

#endif /* __TYPES_H */
