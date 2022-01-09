/*
 * Copyright (C) 2009-2022 Alex Smith
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

#ifdef __ASM__
    #define SUFFIX(x, y)    x
#else
    #define _SUFFIX(x, y)   (x##y)
    #define SUFFIX(x, y)    _SUFFIX(x, y)
#endif

/**
 * Helpers for adding type suffixes to constants in headers shared with
 * assembly code. We cannot use 'ul' and 'ull' suffixes in assembly but they
 * may be needed for C for large constants.
 */
#define UL(x)               (SUFFIX(x, ul))
#define ULL(x)              (SUFFIX(x, ull))

#ifndef __ASM__

#include <compiler.h>
#include <stdalign.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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

/** More atomic type definitions. */
typedef _Atomic(int8_t) atomic_int8_t;
typedef _Atomic(uint8_t) atomic_uint8_t;
typedef _Atomic(int16_t) atomic_int16_t;
typedef _Atomic(uint16_t) atomic_uint16_t;
typedef _Atomic(int32_t) atomic_int32_t;
typedef _Atomic(uint32_t) atomic_uint32_t;
typedef _Atomic(int64_t) atomic_int64_t;
typedef _Atomic(uint64_t) atomic_uint64_t;

/** Internal kernel integer types. */
typedef uint32_t page_num_t;            /**< Integer type representing a number of pages. */

/** Endianness conversion macros. */
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#   define be16_to_cpu(v)   __builtin_bswap16(v)
#   define be32_to_cpu(v)   __builtin_bswap32(v)
#   define be64_to_cpu(v)   __builtin_bswap64(v)
#   define le16_to_cpu(v)   (v)
#   define le32_to_cpu(v)   (v)
#   define le64_to_cpu(v)   (v)
#   define cpu_to_be16(v)   __builtin_bswap16(v)
#   define cpu_to_be32(v)   __builtin_bswap32(v)
#   define cpu_to_be64(v)   __builtin_bswap64(v)
#   define cpu_to_le16(v)   (v)
#   define cpu_to_le32(v)   (v)
#   define cpu_to_le64(v)   (v)
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#   define be16_to_cpu(v)   (v)
#   define be32_to_cpu(v)   (v)
#   define be64_to_cpu(v)   (v)
#   define le16_to_cpu(v)   __builtin_bswap16(v)
#   define le32_to_cpu(v)   __builtin_bswap32(v)
#   define le64_to_cpu(v)   __builtin_bswap64(v)
#   define cpu_to_be16(v)   (v)
#   define cpu_to_be32(v)   (v)
#   define cpu_to_be64(v)   (v)
#   define cpu_to_le16(v)   __builtin_bswap16(v)
#   define cpu_to_le32(v)   __builtin_bswap32(v)
#   define cpu_to_le64(v)   __builtin_bswap64(v)
#else
#   error "__BYTE_ORDER__ is not defined"
#endif

#include <arch/types.h>

#include <kernel/types.h>

#endif /* !__ASM__ */
