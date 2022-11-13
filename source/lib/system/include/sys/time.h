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
 * @brief               POSIX time functions/definitions.
 */

#pragma once

#define __NEED_struct_timeval
#define __NEED_suseconds_t
#define __NEED_time_t
#include <bits/alltypes.h>

#include <sys/select.h>
#include <sys/types.h>

__SYS_EXTERN_C_BEGIN

#define timerisset(t) \
    ((t)->tv_sec || (t)->tv_usec)

#define timerclear(t) \
    ((t)->tv_sec = (t)->tv_usec = 0)

#define timercmp(a, b, op) \
    (((a)->tv_sec == (b)->tv_sec) \
        ? ((a)->tv_usec op (b)->tv_usec) \
        : ((a)->tv_sec op (b)->tv_sec))

#define timeradd(a, b, res) \
    do { \
        (res)->tv_sec = (a)->tv_sec + (b)->tv_sec; \
        (res)->tv_usec = (a)->tv_usec + (b)->tv_usec; \
        if ((res)->tv_usec >= 1000000) { \
            (res)->tv_sec++; \
            (res)->tv_usec -= 1000000; \
        } \
    } while (0)

#define timersub(a, b, res) \
    do { \
        (res)->tv_sec = (a)->tv_sec - (b)->tv_sec; \
        (res)->tv_usec = (a)->tv_usec - (b)->tv_usec; \
        if ((res)->tv_usec < 0) { \
            (res)->tv_sec--; \
            (res)->tv_usec += 1000000; \
        } \
    } while (0)

extern int gettimeofday(struct timeval *tv, void *tz);

extern int utimes(const char *path, const struct timeval times[2]);

__SYS_EXTERN_C_END
