/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Time functions.
 */

#pragma once

#define __NEED_clock_t
#define __NEED_clockid_t
#define __NEED_locale_t
#define __NEED_NULL
#define __NEED_size_t
#define __NEED_struct_timespec
#define __NEED_time_t
#include <bits/alltypes.h>

__SYS_EXTERN_C_BEGIN

/** Structure containing a time. */
struct tm {
    int tm_sec;                     /**< Seconds [0,60]. */
    int tm_min;                     /**< Minutes [0,59]. */
    int tm_hour;                    /**< Hour [0,23]. */
    int tm_mday;                    /**< Day of month [1,31]. */
    int tm_mon;                     /**< Month of year [0,11]. */
    int tm_year;                    /**< Years since 1900. */
    int tm_wday;                    /**< Day of week [0,6] (Sunday = 0). */
    int tm_yday;                    /**< Day of year [0,365]. */
    int tm_isdst;                   /**< Daylight Savings flag. */
};

/** Clock IDs for clock_* functions. */
#define CLOCK_MONOTONIC     0
#define CLOCK_REALTIME      1

extern char *asctime(const struct tm *tm);
extern char *asctime_r(const struct tm *__restrict tm, char *__restrict buf);
//extern clock_t clock(void);
extern char *ctime(const time_t *timep);
//extern double difftime(time_t, time_t);
//extern struct tm *getdate(const char *);
extern struct tm *gmtime(const time_t *timep);
extern struct tm *gmtime_r(const time_t *__restrict timep, struct tm *__restrict tm);
extern struct tm *localtime(const time_t *timep);
extern struct tm *localtime_r(const time_t *__restrict timep, struct tm *__restrict tm);
extern time_t mktime(struct tm *timep);
extern int nanosleep(const struct timespec *rqtp, struct timespec *rmtp);
extern size_t strftime(
    char *__restrict buf, size_t max, const char *__restrict fmt,
    const struct tm *__restrict tm);
//extern size_t strftime_l(char *__restrict, size_t, const char *__restrict,
//  const struct tm *__restrict, locale_t);
//extern char *strptime(const char *__restrict, const char *__restrict, struct tm *__restrict);
extern time_t time(time_t *timep);

//extern int clock_getres(clockid_t clock_id, struct timespec *res);
extern int clock_gettime(clockid_t clock_id, struct timespec *tp);
//extern int clock_settime(clockid_t clock_id, const struct timespec *tp);

// FIXME: Needed for libcxx
#ifdef __cplusplus

extern clock_t clock(void);
extern double difftime(time_t, time_t);

extern size_t strftime_l(
    char *__restrict, size_t, const char *__restrict,
    const struct tm *__restrict, locale_t);

#endif

__SYS_EXTERN_C_END
