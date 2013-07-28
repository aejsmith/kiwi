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
 * @brief		Formatted date/time function.
 */

#include <stdbool.h>
#include <stdio.h>
#include <time.h>

/** Insert a character into a buffer. */
#define PUTCH(b, m, t, c)		\
	if((t) < (m)) { \
		(b)[(t)] = c; \
	} \
	(t)++

/** Format a string into a buffer. */
#define PUTSTR(b, m, t, fmt...)		\
	__extension__ \
	({ \
		int __count = ((int)(m) + 1) - (int)(t); \
		if(__count < 0) { \
			__count = 0; \
		} \
		snprintf(&(b)[(t)], __count, fmt); \
	})

/** Month names (abbreviated). */
static const char *months_abbrev[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
	"Oct", "Nov", "Dec",
};

/** Month names (full). */
static const char *months_full[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December",
};

/** Day names (abbreviated). */
static const char *days_abbrev[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat",
};

/** Day names (full). */
static const char *days_full[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
	"Saturday",
};

/** AM/PM values. */
static const char *am_pm[] = {
	"AM", "am", "PM", "pm",
};

/**
 * Format a date and time.
 *
 * Creates a string describing the date and time in the given tm struct,
 * according to the given format string.
 *
 * @param buf		Buffer to store string in.
 * @param max		Size of the buffer.
 * @param fmt		Format string.
 * @param tm		Time to use.
 *
 * @return		Current time.
 */
size_t strftime(char *restrict buf, size_t max, const char *restrict fmt, const struct tm *restrict tm) {
	bool modif_e, modif_o;
	size_t total = 0;
	int state = 0;
	char ch;

	if(max == 0)
		return 0;

	/* Always leave space for the NULL terminator. */
	max -= 1;

	while(*fmt) {
		ch = *(fmt++);

		switch(state) {
		case 0:
			/* Wait for %. */
			if(ch == '%') {
				state = 1;
			} else {
				PUTCH(buf, max, total, ch);
			}
			break;
		case 1:	
			/* Wait for modifiers. */
			modif_e = modif_o = false;
			state = 2;

			/* Check for literal % and modifiers. */
			switch(ch) {
			case '%':
				/* Literal %. */
				PUTCH(buf, max, total, ch);
				state = 0;
				break;
			case 'E':
				modif_e = true;
				break;
			case 'O':
				modif_o = true;
				break;
			}

			if(state == 0 || modif_e || modif_o)
				break;

			/* Fall through. */
		case 2:
			/* Handle conversion specifiers. */
			switch(ch) {
			case 'a':
				total += PUTSTR(buf, max, total, "%s", days_abbrev[tm->tm_wday]);
				break;
			case 'A':
				total += PUTSTR(buf, max, total, "%s", days_full[tm->tm_wday]);
				break;
			case 'b':
			case 'h':
				total += PUTSTR(buf, max, total, "%s", months_abbrev[tm->tm_mon]);
				break;
			case 'B':
				total += PUTSTR(buf, max, total, "%s", months_full[tm->tm_mon]);
				break;
			case 'd':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_mday);
				break;
			case 'D':
				total += PUTSTR(buf, max, total, "%02d/%02d/%02d", tm->tm_mon + 1, tm->tm_mday, tm->tm_year % 100);
				break;
			case 'e':
				total += PUTSTR(buf, max, total, "%2d", tm->tm_mday);
				break;
			case 'F':
				total += PUTSTR(buf, max, total, "%d-%02d-%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday);
				break;
			case 'H':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_hour);
				break;
			case 'I':
				total += PUTSTR(buf, max, total, "%02d", (tm->tm_hour == 12) ? 12 : tm->tm_hour % 12);
				break;
			case 'j':
				total += PUTSTR(buf, max, total, "%d", tm->tm_yday + 1);
				break;
			case 'n':
				PUTCH(buf, max, total, '\n');
				break;
			case 'm':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_mon + 1);
				break;
			case 'M':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_min);
				break;
			case 'p':
				total += PUTSTR(buf, max, total, "%s", am_pm[tm->tm_hour > 11 ? 2 : 0]);
				break;
			case 'P':
				total += PUTSTR(buf, max, total, "%s", am_pm[tm->tm_hour > 11 ? 3 : 1]);
				break;
			case 'S':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_sec);
				break;
			case 'y':
				total += PUTSTR(buf, max, total, "%02d", tm->tm_year % 100);
				break;
			case 'Y':
				total += PUTSTR(buf, max, total, "%d", tm->tm_year + 1900);
				break;
			case 'z':
				total += PUTSTR(buf, max, total, "+0000");
				break;
			case 'Z':
				total += PUTSTR(buf, max, total, "UTC");
				break;
			}
			state = 0;
			break;
		}
	}

	buf[total] = 0;
	return (total <= max) ? total : 0;
}
