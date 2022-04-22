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
 * @brief               Standard type defintions.
 */

#include <system/defs.h>

// TODO: This file is added for musl compatibility. We should move more stuff
// into here.

#define __BYTE_ORDER        __BYTE_ORDER__
#define __LITTLE_ENDIAN     __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN        __ORDER_BIG_ENDIAN__

#if defined(__NEED_int8_t) && !defined(__DEFINED_int8_t)
    typedef signed char int8_t;
    #define __DEFINED_int8_t
#endif

#if defined(__NEED_int16_t) && !defined(__DEFINED_int16_t)
    typedef signed short int16_t;
    #define __DEFINED_int16_t
#endif

#if defined(__NEED_int32_t) && !defined(__DEFINED_int32_t)
    typedef signed int int32_t;
    #define __DEFINED_int32_t
#endif

#if defined(__NEED_int64_t) && !defined(__DEFINED_int64_t)
    #if __WORDSIZE == 64
        typedef signed long int64_t;
    #else
        typedef signed long long int64_t;
    #endif
    #define __DEFINED_int64_t
#endif

#if defined(__NEED_uint8_t) && !defined(__DEFINED_uint8_t)
    typedef unsigned char uint8_t;
    #define __DEFINED_uint8_t
#endif

#if defined(__NEED_uint16_t) && !defined(__DEFINED_uint16_t)
    typedef unsigned short uint16_t;
    #define __DEFINED_uint16_t
#endif

#if defined(__NEED_uint32_t) && !defined(__DEFINED_uint32_t)
    typedef unsigned int uint32_t;
    #define __DEFINED_uint32_t
#endif

#if defined(__NEED_uint64_t) && !defined(__DEFINED_uint64_t)
    #if __WORDSIZE == 64
        typedef unsigned long uint64_t;
    #else
        typedef unsigned long long uint64_t;
    #endif
    #define __DEFINED_uint64_t
#endif

#if defined(__x86_64__)
    #if defined(__FLT_EVAL_METHOD__) && __FLT_EVAL_METHOD__ == 2
        #if defined(__NEED_float_t) && !defined(__DEFINED_float_t)
            typedef long double float_t;
            #define __DEFINED_float_t
        #endif

        #if defined(__NEED_double_t) && !defined(__DEFINED_double_t)
            typedef long double double_t;
            #define __DEFINED_double_t
        #endif
    #endif
#endif

#if defined(__NEED_float_t) && !defined(__DEFINED_float_t)
    typedef float float_t;
    #define __DEFINED_float_t
#endif

#if defined(__NEED_double_t) && !defined(__DEFINED_double_t)
    typedef double double_t;
    #define __DEFINED_double_t
#endif
