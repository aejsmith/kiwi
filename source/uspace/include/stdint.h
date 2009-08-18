/* Kiwi C library - Standard integer definitions
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Standard integer definitions.
 */

#ifndef __STDINT_H
#define __STDINT_H

#ifdef __cplusplus
extern "C" {
#endif

typedef signed char int8_t;
typedef signed short int16_t;
typedef signed int int32_t;
typedef signed long long int64_t;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

typedef signed char int_least8_t;
typedef signed short int_least16_t;
typedef signed int int_least32_t;
typedef signed long long int_least64_t;
typedef unsigned char uint_least8_t;
typedef unsigned short uint_least16_t;
typedef unsigned int uint_least32_t;
typedef unsigned long long uint_least64_t;

typedef signed char int_fast8_t;
typedef signed short int_fast16_t;
typedef signed int int_fast32_t;
typedef signed long long int_fast64_t;
typedef unsigned char uint_fast8_t;
typedef unsigned short uint_fast16_t;
typedef unsigned int uint_fast32_t;
typedef unsigned long long uint_fast64_t;

typedef signed long intptr_t;
typedef unsigned long uintptr_t;

typedef signed long intmax_t;
typedef unsigned long uintmax_t;

/* ISO C99 specifies that in C++ these macros should only be defined
 * if explicitly requested */
#if !defined(__cplusplus) || defined(__STDC_LIMIT_MACROS)

#ifndef INT8_MIN
# define INT8_MIN		(-0x80)
# define INT8_MAX		0x7F
# define UINT8_MAX		0xFFu
#endif

#ifndef INT16_MIN
# define INT16_MIN		(-0x8000)
# define INT16_MAX		0x7FFF
# define UINT16_MAX		0xFFFFu
#endif

#ifndef INT32_MIN
# define INT32_MIN		(-0x80000000)
# define INT32_MAX		0x7FFFFFFF
# define UINT32_MAX		0xFFFFFFFFu
#endif

#define INT64_MIN		(-0x8000000000000000ll)
#define INT64_MAX		0x7FFFFFFFFFFFFFFFll
#define UINT64_MAX		0xFFFFFFFFFFFFFFFFull

#define INT_LEAST8_MIN		INT8_MIN
#define INT_LEAST8_MAX		INT8_MAX
#define UINT_LEAST8_MAX		UINT8_MAX

#define INT_LEAST16_MIN		INT16_MIN
#define INT_LEAST16_MAX		INT16_MAX
#define UINT_LEAST16_MAX	UINT16_MAX

#define INT_LEAST32_MIN		INT32_MIN
#define INT_LEAST32_MAX		INT32_MAX
#define UINT_LEAST32_MAX	UINT32_MAX

#define INT_LEAST64_MIN		INT64_MIN
#define INT_LEAST64_MAX		INT64_MAX
#define UINT_LEAST64_MAX	UINT64_MAX

#define INT_FAST8_MIN		INT8_MIN
#define INT_FAST8_MAX		INT8_MAX
#define UINT_FAST8_MAX		UINT8_MAX

#define INT_FAST16_MIN		INT16_MIN
#define INT_FAST16_MAX		INT16_MAX
#define UINT_FAST16_MAX		UINT16_MAX

#define INT_FAST32_MIN		INT32_MIN
#define INT_FAST32_MAX		INT32_MAX
#define UINT_FAST32_MAX		UINT32_MIN

#define INT_FAST64_MIN		INT64_MIN
#define INT_FAST64_MAX		INT64_MAX
#define UINT_FAST64_MAX		UINT64_MIN

/* FIXME */
//#include <arch/stdint.h>

#endif /* C++/limit macros */

/* Same as above */
#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)

#define INT8_C(x)		((int_least8_t)x)
#define INT16_C(x)		((int_least16_t)x)
#define INT32_C(x)		((int_least32_t)x)
#define INT64_C(x)		((int_least64_t)x ## LL)

#define UINT8_C(x)		((uint_least8_t)x)
#define UINT16_C(x)		((uint_least16_t)x)
#define UINT32_C(x)		((uint_least32_t)x)
#define UINT64_C(x)		((uint_least64_t)x ## ULL)

#define INTMAX_C(x)		((intmax_t)x ## LL)
#define UNITMAX_C(x)		((uintmax_t)x ## ULL)

#endif /* C++/constant macros */

#ifdef __cplusplus
}
#endif

#endif /* __STDINT_H */
