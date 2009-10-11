/* String to double function
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
 * @brief		String to double function.
 */

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/** Convert a string to a double precision number.
 *
 * Converts a string to a double-precision number.
 *
 * @param s		String to convert.
 * @param endptr	Pointer to store end of string in (can be NULL).
 *
 * @return		Converted number.
 */
double strtod(const char *s, char **endptr) {
	const char *p = s;
	long double factor, value = 0.L;
	int sign = 1;
	unsigned int expo = 0;

	while(isspace(*p)) {
		p++;
	}

	switch(*p) {
	case '-':
		sign = -1;
	case '+':
		p++;
	default:
		break;
	}

	while((unsigned int)(*p - '0') < 10u) {
		value = value * 10 + (*p++ - '0');
	}

	if(*p == '.') {
		factor = 1.;
		p++;

		while((unsigned int)(*p - '0') < 10u) {
			factor *= 0.1;
			value += (*p++ - '0') * factor;
		}
	}

	if((*p | 32) == 'e') {
		factor = 10.L;

		switch (*++p) {
		case '-':
			factor = 0.1;
		case '+':
			p++;
			break;
		case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
			break;
		default:
			value = 0.L;
			p = s;
			goto done;
		}

		while((unsigned int)(*p - '0') < 10u) {
			expo = 10 * expo + (*p++ - '0');
		}

		while(1) {
			if(expo & 1) {
				value *= factor;
			}
			if((expo >>= 1) == 0) {
				break;
			}
			factor *= factor;
		}
	}
done:
	if(endptr != NULL) {
		*endptr = (char *)p;
	}

	return value * sign;
}
