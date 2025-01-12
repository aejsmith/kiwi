/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
