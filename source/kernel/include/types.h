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
 * @brief               Type definitions.
 */

#pragma once

#include <compiler.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#if CONFIG_64BIT
#   define __PRI_64     "l"
#else
#   define __PRI_64     "ll"
#endif

/** Format character definitions for printf(). */
#define PRIu8           "u"             /**< Format for uint8_t. */
#define PRIu16          "u"             /**< Format for uint16_t. */
#define PRIu32          "u"             /**< Format for uint32_t. */
#define PRIu64          __PRI_64 "u"    /**< Format for uint64_t. */
#define PRId8           "d"             /**< Format for int8_t. */
#define PRId16          "d"             /**< Format for int16_t. */
#define PRId32          "d"             /**< Format for int32_t. */
#define PRId64          __PRI_64 "d"    /**< Format for int64_t. */
#define PRIx8           "x"             /**< Format for (u)int8_t (hexadecimal). */
#define PRIx16          "x"             /**< Format for (u)int16_t (hexadecimal). */
#define PRIx32          "x"             /**< Format for (u)int32_t (hexadecimal). */
#define PRIx64          __PRI_64 "x"    /**< Format for (u)int64_t (hexadecimal). */
#define PRIo8           "o"             /**< Format for (u)int8_t (octal). */
#define PRIo16          "o"             /**< Format for (u)int16_t (octal). */
#define PRIo32          "o"             /**< Format for (u)int32_t (octal). */
#define PRIo64          __PRI_64 "o"    /**< Format for (u)int64_t (octal). */

/** Internal kernel integer types. */
typedef uint32_t page_num_t;            /**< Integer type representing a number of pages. */

/** Number of bits in a char. */
#define CHAR_BIT        8

/** Minimum and maximum values a signed char can hold. */
#define SCHAR_MIN       (-SCHAR_MAX - 1)
#define SCHAR_MAX       __SCHAR_MAX__

/** Maximum value an unsigned char can hold. */
#define UCHAR_MAX       ((SCHAR_MAX * 2) + 1)

/** Minimum and maximum values a char can hold. */
#ifdef __CHAR_UNSIGNED__
#   define CHAR_MIN     0
#   define CHAR_MAX     UCHAR_MAX
#else
#   define CHAR_MIN     SCHAR_MIN
#   define CHAR_MAX     SCHAR_MAX
#endif

/** Minimum and maximum values a signed short can hold. */
#define SHRT_MIN        (-SHRT_MAX - 1)
#define SHRT_MAX        __SHRT_MAX__

/** Maximum value an unsigned short can hold. */
#define USHRT_MAX       ((SHRT_MAX * 2) + 1)

/** Minimum and maximum values a signed int can hold. */
#define INT_MIN         (-INT_MAX - 1)
#define INT_MAX         __INT_MAX__

/** Maximum value an unsigned int can hold. */
#define UINT_MAX        ((INT_MAX * 2U) + 1U)

/** Minimum and maximum values a signed long can hold. */
#define LONG_MIN        (-LONG_MAX - 1L)
#define LONG_MAX        __LONG_MAX__

/** Maximum value an unsigned long can hold. */
#define ULONG_MAX       ((LONG_MAX * 2UL) + 1UL)

/** Minimum and maximum values a signed long long can hold. */
#define LLONG_MIN       (-LLONG_MAX - 1LL)
#define LLONG_MAX       __LONG_LONG_MAX__

/** Maximum value an unsigned long long can hold. */
#define ULLONG_MAX      ((LLONG_MAX * 2ULL) + 1ULL)

#include <arch/types.h>

#include <kernel/types.h>
