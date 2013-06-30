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
char *asctime_r(const struct tm *restrict tm, char *restrict buf) {
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
