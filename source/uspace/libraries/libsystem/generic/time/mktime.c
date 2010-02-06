/*
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
 * @brief		UNIX time function.
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

/** Convert a time to a UNIX time.
 *
 * Converts the time described by the given time structure to a UNIX
 * timestamp.
 *
 * @param timep		Time to convert.
 *
 * @return		UNIX timestamp.
 */
time_t mktime(struct tm *timep) {
	time_t time = 0;
	int i;

	/* Start by adding the time of day and day of month together. */
	time += timep->tm_sec;
	time += timep->tm_min * 60;
	time += timep->tm_hour * 60 * 60;
	time += (timep->tm_mday - 1) * 24 * 60 * 60;

	/* Convert the month into days. */
	time += days_before_month[timep->tm_mon] * 24 * 60 * 60;

	/* If this year is a leap year, and we're past February, we need to
	 * add another day. */
	if(timep->tm_mon > 1 && LEAPYR(timep->tm_year + 1900)) {
		time += 24 * 60 * 60;
	}

	/* Add the days in each year before this year from 1970. */
	for(i = 1970; i < (timep->tm_year + 1900); i++) {
		time += DAYS(i) * 24 * 60 * 60;
	}

	return time;
}
