/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Time functions.
 */

#ifndef __TIME_H
#define __TIME_H

#include <sys/types.h>
#define __need_NULL
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Time specification structure. */
struct timespec {
	time_t tv_sec;			/**< Seconds. */
	long tv_nsec;			/**< Additional nanoseconds since. */
};

/** Structure containing a time. */
struct tm {
	int tm_sec;			/**< Seconds [0,60]. */
	int tm_min;			/**< Minutes [0,59]. */
	int tm_hour;			/**< Hour [0,23]. */
	int tm_mday;			/**< Day of month [1,31]. */
	int tm_mon;			/**< Month of year [0,11]. */
	int tm_year;			/**< Years since 1900. */
	int tm_wday;			/**< Day of week [0,6] (Sunday = 0). */
	int tm_yday;			/**< Day of year [0,365]. */
	int tm_isdst;			/**< Daylight Savings flag. */
};

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
extern size_t strftime(char *__restrict buf, size_t max, const char *__restrict fmt, const struct tm *__restrict tm);
//extern char *strptime(const char *__restrict, const char *__restrict, struct tm *__restrict);
extern time_t time(time_t *timep);

// FIXME: Needed for libstdc++ compilation.
#ifdef __cplusplus
extern clock_t clock(void);
extern double difftime(time_t, time_t);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TIME_H */