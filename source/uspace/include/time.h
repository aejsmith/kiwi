/* Time functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Time functions.
 */

#ifndef __TIME_H
#define __TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#define __need_size_t
#include <stddef.h>
#include <stdint.h>

/** Type containing a UNIX timestamp. */
typedef int64_t time_t;

/** Type containing clock ticks since process start. */
typedef int64_t clock_t;

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
extern char *asctime_r(const struct tm *tm, char *buf);
//extern clock_t clock(void);
extern char *ctime(const time_t *timep);
//extern double difftime(time_t, time_t);
//extern struct tm *getdate(const char *);
extern struct tm *gmtime(const time_t *timep);
extern struct tm *gmtime_r(const time_t *timep, struct tm *tm);
extern struct tm *localtime(const time_t *timep);
extern struct tm *localtime_r(const time_t *timep, struct tm *tm);
extern time_t mktime(struct tm *timep);
extern size_t strftime(char *buf, size_t max, const char *fmt, const struct tm *tm);
//extern char *strptime(const char *, const char *, struct tm *);
//extern time_t time(time_t *timep);

#ifdef __cplusplus
extern clock_t clock(void);
extern double difftime(time_t, time_t);
extern time_t time(time_t *timep);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __TIME_H */
