/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Standard integer definitions.
 */

#pragma once

#define __NEED_int8_t
#define __NEED_int16_t
#define __NEED_int32_t
#define __NEED_int64_t
#define __NEED_uint8_t
#define __NEED_uint16_t
#define __NEED_uint32_t
#define __NEED_uint64_t
#include <bits/alltypes.h>

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

#define __INT8_C(x)         x
#define __INT16_C(x)        x
#define __INT32_C(x)        x
#if __WORDSIZE == 64
#   define __INT64_C(x)     x ## l
#else
#   define __INT64_C(x)     x ## ll
#endif

#define __UINT8_C(x)        x ## u
#define __UINT16_C(x)       x ## u
#define __UINT32_C(x)       x ## u
#if __WORDSIZE == 64
#   define __UINT64_C(x)    x ## ul
#else
#   define __UINT64_C(x)    x ## ull
#endif

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
#define UINT_FAST32_MAX     UINT32_MAX

#define INT_FAST64_MIN      INT64_MIN
#define INT_FAST64_MAX      INT64_MAX
#define UINT_FAST64_MAX     UINT64_MAX

#define INTMAX_MIN          INT64_MIN
#define INTMAX_MAX          INT64_MAX
#define UINTMAX_MAX         UINT64_MAX

#if __WORDSIZE == 64
#   define SIZE_MAX         UINT64_MAX
#else
#   define SIZE_MAX         UINT32_MAX
#endif

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
