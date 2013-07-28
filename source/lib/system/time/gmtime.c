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
 * @brief		UTC time function.
 */

#include <time.h>

/** Check if a year is a leap year. */
#define LEAPYR(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/** Get number of days in a year. */
#define DAYS(y)		(LEAPYR(y) ? 366 : 365)

/** Table containing number of days before a month. */
static int days_before_month[12] = {
	/* Jan. */ 0,
	/* Feb. */ 31,
	/* Mar. */ 31 + 28,
	/* Apr. */ 31 + 28 + 31,
	/* May. */ 31 + 28 + 31 + 30,
	/* Jun. */ 31 + 28 + 31 + 30 + 31,
	/* Jul. */ 31 + 28 + 31 + 30 + 31 + 30,
	/* Aug. */ 31 + 28 + 31 + 30 + 31 + 30 + 31,
	/* Sep. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31,
	/* Oct. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30,
	/* Nov. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31,
	/* Dec. */ 31 + 28 + 31 + 30 + 31 + 30 + 31 + 31 + 30 + 31 + 30,
};

static struct tm __gmtime_tm;

/**
 * Get the UTC time.
 *
 * Gets the UTC time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep		Time to use.
 * @param tm		Time structure to fill in.
 *
 * @return		Pointer to time structure filled in.
 */
struct tm *gmtime_r(const time_t *restrict timep, struct tm *restrict tm) {
	time_t i, time;

	time = *timep % (24 * 60 * 60);

	tm->tm_sec = time % 60;
	tm->tm_min = (time / 60) % 60;
	tm->tm_hour = (time / 60) / 60;

	time = *timep / (24 * 60 * 60);

	/* Add 4 because January 1st 1970 == Thursday, 4th day of week. */
	tm->tm_wday = (4 + time) % 7;

	for(i = 1970; time >= DAYS(i); i++)
		time -= DAYS(i);

	tm->tm_year = i - 1900;
	tm->tm_yday = time;

	tm->tm_mday = 1;
	if(LEAPYR(i) && (time >= days_before_month[2])) {
		if(time == days_before_month[2])
			tm->tm_mday = 2;

		time -= 1;
	}

	for(i = 11; i && (days_before_month[i] > time); i--);

	tm->tm_mon = i;
	tm->tm_mday += time - days_before_month[i];
	return tm;
}

/**
 * Get the UTC time.
 *
 * Gets the UTC time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep		Time to use.
 *
 * @return		Pointer to time structure filled in.
 */
struct tm *gmtime(const time_t *timep) {
	return gmtime_r(timep, &__gmtime_tm);
}
