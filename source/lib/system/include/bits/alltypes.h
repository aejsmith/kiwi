/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * This file is partly derived from musl. See 3rdparty/lib/musl/COPYRIGHT for
 * license.
 */

/**
 * @file
 * @brief               Standard type defintions.
 */

#include <system/defs.h>

/**
 * Compiler definitions, supported here for musl compatibility.
 */

#if defined(__NEED_NULL) || \
    defined(__NEED_size_t) || \
    defined(__NEED_ptrdiff_t) || \
    defined(__NEED_wchar_t) || \
    defined(__NEED_wint_t)
    #if defined(__NEED_NULL)
        #define __need_NULL
    #endif

    #if defined(__NEED_size_t)
        #define __need_size_t
    #endif

    #if defined(__NEED_ptrdiff_t)
        #define __need_ptrdiff_t
    #endif

    #if defined(__NEED_wchar_t)
        #define __need_wchar_t
    #endif

    #if defined(__NEED_wint_t)
        #define __need_wint_t
    #endif

    #include <stddef.h>
#endif

#if defined(__NEED_va_list) || defined(__NEED___isoc_va_list)
    #include <stdarg.h>

    #if defined(__NEED___isoc_va_list) && !defined(__DEFINED___isoc_va_list)
        typedef va_list __isoc_va_list;
        #define __DEFINED___isoc_va_list
    #endif
#endif

/**
 * Other definitions.
 */

#define __BYTE_ORDER        __BYTE_ORDER__
#define __LITTLE_ENDIAN     __ORDER_LITTLE_ENDIAN__
#define __BIG_ENDIAN        __ORDER_BIG_ENDIAN__

#if __WORDSIZE == 64
    #define __int64 long
#else
    #define __int64 long long
#endif

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
    typedef signed __int64 int64_t;
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
    typedef unsigned __int64 uint64_t;
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

#if defined(__NEED_time_t) && !defined(__DEFINED_time_t)
    typedef __int64 time_t;
    #define __DEFINED_time_t
#endif

#if defined(__NEED_clock_t) && !defined(__DEFINED_clock_t)
    typedef __int64 clock_t;
    #define __DEFINED_clock_t
#endif

#if defined(__NEED_pid_t) && !defined(__DEFINED_pid_t)
    typedef int pid_t;
    #define __DEFINED_pid_t
#endif

#if defined(__NEED_off_t) && !defined(__DEFINED_off_t)
    typedef __int64 off_t;
    #define __DEFINED_off_t
#endif

#if defined(__NEED_mode_t) && !defined(__DEFINED_mode_t)
    typedef unsigned int mode_t;
    #define __DEFINED_mode_t
#endif

#if defined(__NEED_suseconds_t) && !defined(__DEFINED_suseconds_t)
    typedef __int64 suseconds_t;
    #define __DEFINED_suseconds_t
#endif

#if defined(__NEED_useconds_t) && !defined(__DEFINED_useconds_t)
    typedef unsigned __int64 useconds_t;
    #define __DEFINED_useconds_t
#endif

#if defined(__NEED_blkcnt_t) && !defined(__DEFINED_blkcnt_t)
    typedef int blkcnt_t;
    #define __DEFINED_blkcnt_t
#endif

#if defined(__NEED_blksize_t) && !defined(__DEFINED_blksize_t)
    typedef int blksize_t;
    #define __DEFINED_blksize_t
#endif

#if defined(__NEED_dev_t) && !defined(__DEFINED_dev_t)
    typedef unsigned int dev_t;
    #define __DEFINED_dev_t
#endif

#if defined(__NEED_ino_t) && !defined(__DEFINED_ino_t)
    typedef unsigned __int64 ino_t;
    #define __DEFINED_ino_t
#endif

#if defined(__NEED_nlink_t) && !defined(__DEFINED_nlink_t)
    typedef unsigned int nlink_t;
    #define __DEFINED_nlink_t
#endif

#if defined(__NEED_uid_t) && !defined(__DEFINED_uid_t)
    typedef unsigned int uid_t;
    #define __DEFINED_uid_t
#endif

#if defined(__NEED_gid_t) && !defined(__DEFINED_gid_t)
    typedef unsigned int gid_t;
    #define __DEFINED_gid_t
#endif

#if defined(__NEED_clockid_t) && !defined(__DEFINED_clockid_t)
    typedef unsigned int clockid_t;
    #define __DEFINED_clockid_t
#endif

#if defined(__NEED_wctype_t) && !defined(__DEFINED_wctype_t)
    typedef unsigned long wctype_t;
    #define __DEFINED_wctype_t
#endif

#if defined(__NEED_locale_t) && !defined(__DEFINED_locale_t)
    typedef struct __locale_struct *locale_t;
    #define __DEFINED_locale_t
#endif

#if defined(__NEED_FILE) && !defined(__DEFINED_FILE)
    typedef struct __fstream_internal FILE;
    #define __DEFINED_FILE
#endif

#if defined(__NEED_mbstate_t) && !defined(__DEFINED_mbstate_t)
    typedef struct __mbstate_t {
        unsigned __opaque1;
        unsigned __opaque2;
    } mbstate_t;

    #define __DEFINED_mbstate_t
#endif

#if defined(__NEED_struct_timespec) && !defined(__DEFINED_struct_timespec)
    /** Time specification structure. */
    struct timespec {
        time_t tv_sec;                  /**< Seconds. */
        long tv_nsec;                   /**< Additional nanoseconds since. */
    };

    #define __DEFINED_struct_timespec
#endif

#if defined(__NEED_struct_timeval) && !defined(__DEFINED_struct_timeval)
    /** Time value structure. */
    struct timeval {
        time_t tv_sec;                  /**< Seconds. */
        suseconds_t tv_usec;            /**< Additional microseconds since. */
    };

    #define __DEFINED_struct_timeval
#endif

#undef __int64
