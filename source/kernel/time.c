/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Time handling functions.
 *
 * @todo		For the moment, this code assumes that the hardware
 *			RTC is storing the time as UTC.
 */

#include <time.h>

/** Check if a year is a leap year. */
#define LEAPYR(y)	(((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))

/** Get number of days in a year. */
#define DAYS(y)		(LEAPYR(y) ? 366 : 365)

/** Table containing number of days before a month. */
static int days_before_month[] = {
	0,
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

/** The number of microseconds since the Epoch the kernel was booted at. */
static useconds_t boot_unix_time = 0;

/** Convert a date/time to microseconds since the epoch.
 * @param year		Year.
 * @param month		Month (1-12).
 * @param day		Day of month (1-31).
 * @param hour		Hour (0-23).
 * @param min		Minute (0-59).
 * @param sec		Second (0-59).
 * @return		Number of microseconds since the epoch. */
useconds_t time_to_unix(int year, int month, int day, int hour, int min, int sec) {
	uint32_t seconds = 0;
	int i;

	/* Start by adding the time of day and day of month together. */
	seconds += sec;
	seconds += min * 60;
	seconds += hour * 60 * 60;
	seconds += (day - 1) * 24 * 60 * 60;

	/* Convert the month into days. */
	seconds += days_before_month[month] * 24 * 60 * 60;

	/* If this year is a leap year, and we're past February, we need to
	 * add another day. */
	if(month > 2 && LEAPYR(year)) {
		seconds += 24 * 60 * 60;
	}

	/* Add the days in each year before this year from 1970. */
	for(i = 1970; i < year; i++) {
		seconds += DAYS(i) * 24 * 60 * 60;
	}

	return SECS2USECS(seconds);
}

/** Get the number of microseconds since the Unix Epoch.
 *
 * Returns the number of microseconds that have passed since the Unix epoch,
 * 00:00:00 UTC, January 1st, 1970.
 *
 * @return		Number of microseconds since epoch.
 */
useconds_t time_since_epoch(void) {
	return boot_unix_time + time_since_boot();
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to spin for. */
void spin(useconds_t us) {
	useconds_t target = time_since_boot() + us;
	while(time_since_boot() < target) {
		__asm__ volatile("pause");
	}
}

/** Initialise the timing system. */
void __init_text time_init(void) {
	/* Initialise the boot time. */
	boot_unix_time = time_from_hardware() - time_since_boot();
}
