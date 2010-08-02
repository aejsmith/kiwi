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
 * @brief		Local time function.
 */

#include <time.h>

static struct tm __localtime_tm;

/** Get the local time.
 *
 * Gets the local time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep		Time to use.
 * @param tm		Time structure to fill in.
 *
 * @return		Pointer to time structure filled in.
 */
struct tm *localtime_r(const time_t *timep, struct tm *tm) {
	return gmtime_r(timep, tm);
}

/** Get the local time.
 *
 * Gets the local time equivalent of the given timestamp (seconds from the
 * UNIX epoch).
 *
 * @param timep		Time to use.
 *
 * @return		Pointer to time structure filled in.
 */
struct tm *localtime(const time_t *timep) {
	return localtime_r(timep, &__localtime_tm);
}
