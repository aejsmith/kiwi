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
 * @brief		Time/date to string conversion function.
 */

#include <time.h>

/** Buffer for asctime(). */
static char asctime_buf[64];

/** Convert time/date to string.
 *
 * Converts the time and date described in the given time structure to a
 * string representation.
 *
 * @param tm		Time structure.
 * @param buf		Buffer to store string (should be 26 bytes long).
 *
 * @return		Pointer to supplied buffer (or NULL on failure).
 */
char *asctime_r(const struct tm *tm, char *buf) {
	strftime(buf, 26, "%a %b %d %H:%M:%S %Y\n", tm);
	return buf;
}

/** Convert time/date to string.
 *
 * Converts the time and date described in the given time structure to a
 * string representation. The returned string is statically allocated and
 * may be overwritten by a later call to asctime() or ctime().
 *
 * @param tm		Time structure.
 *
 * @return		Pointer to converted string (or NULL on failure).
 */
char *asctime(const struct tm *tm) {
	return asctime_r(tm, asctime_buf);
}

/** Convert time/date to string.
 *
 * Converts the time and date described in the given UNIX timestamp to a
 * string representation. The returned string is statically allocated and
 * may be overwritten by a later call to asctime() or ctime().
 *
 * @param timep		Pointer to timestamp to convert.
 *
 * @return		Pointer to converted string (or NULL on failure).
 */
char *ctime(const time_t *timep) {
	return asctime(localtime(timep));
}
