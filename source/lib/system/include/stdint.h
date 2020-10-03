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
 * @brief               Standard integer definitions.
 */

#pragma once

#include <system/defs.h>

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
#if __WORDSIZE == 64
    typedef signed long int64_t;
#else
    typedef signed long long int64_t;
#endif

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
#if __WORDSIZE == 64
    typedef unsigned long uint64_t;
#else
    typedef unsigned long long uint64_t;
#endif

typedef signed char int_least8_t;
typedef signed short int_least16_t;
typedef signed int int_least32_t;
#if __WORDSIZE == 64
    typedef signed long int_least64_t;
#else
    typedef signed long long int_least64_t;
#endif

typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned int uint_least32_t;
#if __WORDSIZE == 64
    typedef unsigned long uint_least64_t;
#else
    typedef unsigned long long uint_least64_t;
#endif

typedef signed char int_fast8_t;
typedef signed short int_fast16_t;
typedef signed int int_fast32_t;
#if __WORDSIZE == 64
    typedef signed long int_fast64_t;
#else
    typedef signed long long int_fast64_t;
#endif

typedef unsigned char uint_fast8_t;
typedef unsigned short uint_fast16_t;
typedef unsigned int uint_fast32_t;
#if __WORDSIZE == 64
    typedef unsigned long uint_fast64_t;
#else
    typedef unsigned long long uint_fast64_t;
#endif

typedef signed long intptr_t;
typedef unsigned long uintptr_t;

typedef int64_t intmax_t;
typedef uint64_t uintmax_t;

#define __INT8_C(x)         ((int_least8_t)x)
#define __INT16_C(x)        ((int_least16_t)x)
#define __INT32_C(x)        ((int_least32_t)x)
#if __WORDSIZE == 64
#   define __INT64_C(x)     ((int_least64_t)x ## l)
#else
#   define __INT64_C(x)     ((int_least64_t)x ## ll)
#endif

#define __UINT8_C(x)        ((uint_least8_t)x)
#define __UINT16_C(x)       ((uint_least16_t)x)
#define __UINT32_C(x)       ((uint_least32_t)x)
#if __WORDSIZE == 64
#   define __UINT64_C(x)    ((uint_least64_t)x ## ul)
#else
#   define __UINT64_C(x)    ((uint_least64_t)x ## ull)
#endif

/* ISO C99 specifies that in C++ these macros should only be defined if
 * explicitly requested */
#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)

#define INT8_MIN            (-128)
#define INT8_MAX            127
#define UINT8_MAX           255

#define INT16_MIN           (-32767-1)
#define INT16_MAX           32767
#define UINT16_MAX          65535

#define INT32_MIN           (-2147483647-1)
#define INT32_MAX           2147483647
#define UINT32_MAX          4294967295

#define INT64_MIN           (-__INT64_C(9223372036854775807)-1)
#define INT64_MAX           __INT64_C(9223372036854775807)
#define UINT64_MAX          __UINT64_C(18446744073709551615)

#define INT_LEAST8_MIN      INT8_MIN
#define INT_LEAST8_MAX      INT8_MAX
#define UINT_LEAST8_MAX     UINT8_MAX

#define INT_LEAST16_MIN     INT16_MIN
#define INT_LEAST16_MAX     INT16_MAX
#define UINT_LEAST16_MAX    UINT16_MAX

#define INT_LEAST32_MIN     INT32_MIN
#define INT_LEAST32_MAX     INT32_MAX
#define UINT_LEAST32_MAX    UINT32_MAX

#define INT_LEAST64_MIN     INT64_MIN
#define INT_LEAST64_MAX     INT64_MAX
#define UINT_LEAST64_MAX    UINT64_MAX

#define INT_FAST8_MIN       INT8_MIN
#define INT_FAST8_MAX       INT8_MAX
#define UINT_FAST8_MAX      UINT8_MAX

#define INT_FAST16_MIN      INT16_MIN
#define INT_FAST16_MAX      INT16_MAX
#define UINT_FAST16_MAX     UINT16_MAX

#define INT_FAST32_MIN      INT32_MIN
#define INT_FAST32_MAX      INT32_MAX
#define UINT_FAST32_MAX     UINT32_MIN

#define INT_FAST64_MIN      INT64_MIN
#define INT_FAST64_MAX      INT64_MAX
#define UINT_FAST64_MAX     UINT64_MIN

#if __WORDSIZE == 64
#   define SIZE_MAX         UINT64_MAX
#else
#   define SIZE_MAX         UINT32_MAX
#endif

#endif

/* Same as above */
#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)

#define INT8_C(x)           __INT8_C(x)
#define INT16_C(x)          __INT16_C(x)
#define INT32_C(x)          __INT32_C(x)
#define INT64_C(x)          __INT64_C(x)

#define UINT8_C(x)          __UINT8_C(x)
#define UINT16_C(x)         __UINT16_C(x)
#define UINT32_C(x)         __UINT32_C(x)
#define UINT64_C(x)         __UINT64_C(x)

#define INTMAX_C(x)         __INT64_C(x)
#define UINTMAX_C(x)        __UINT64_C(x)

#endif
