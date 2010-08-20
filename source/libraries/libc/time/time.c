/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Current time function.
 */

#include <sys/time.h>
#include <time.h>

/** Get the current time as seconds since the UNIX epoch.
 * @param timep		If not NULL, result will also be stored here.
 * @return		Current time. */
time_t time(time_t *timep) {
	struct timeval tv;

	if(gettimeofday(&tv, NULL) != 0) {
		return -1;
	}

	if(timep) {
		*timep = tv.tv_sec;
	}
	return tv.tv_sec;
}
