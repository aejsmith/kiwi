/*
 * Copyright (C) 2007-2009 Alex Smith
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
 * @brief		String to integer functions.
 */

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>

/** Macro to implement strtoul() and strtoull(). */
#define __strtoux(type, max, cp, endp, base) \
	__extension__ \
	({ \
		type result = 0, value; \
		if(!base) { \
			if(*cp == '0') { \
				if((tolower(*(++cp)) == 'x') && isxdigit(cp[1])) { \
					cp++; \
					base = 16; \
				} else { \
					base = 8; \
				} \
			} else { \
				base = 10; \
			} \
		} else if(base == 8) { \
			if(cp[0] == '0') { \
				cp++; \
			} \
		} else if(base == 16) { \
			if(cp[0] == '0' && tolower(cp[1]) == 'x') { \
				cp += 2; \
			} \
		} else if(base != 10) { \
			errno = ERR_PARAM_INVAL; \
			return max; \
		} \
		while(isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0' : tolower(*cp) - 'a' + 10) < (unsigned int)base) { \
			result = result * (unsigned int)base + value; \
			cp++; \
		} \
		if(endp) { \
			*endp = (char *)cp; \
		} \
		result; \
	})

/** Convert a string to an unsigned long.
 *
 * Converts a string to an unsigned long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use (if zero will guess).
 *
 * @return		Converted value.
 */
unsigned long strtoul(const char *cp, char **endp, int base) {
	return __strtoux(unsigned long, ULONG_MAX, cp, endp, base);
}

/** Convert a string to a signed long.
 *
 * Converts a string to an signed long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
long strtol(const char *cp, char **endp, int base) {
	if(*cp == '-') {
		return -strtoul(cp + 1, endp, base);
	}
	return strtoul(cp, endp, base);
}

/** Convert a string to an unsigned long long.
 *
 * Converts a string to an unsigned long long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
unsigned long long int strtoull(const char *cp, char **endp, int base) {
	return __strtoux(unsigned long long int, ULLONG_MAX, cp, endp, base);
}

/** Convert a string to an signed long long.
 *
 * Converts a string to an signed long long using the specified number base.
 *
 * @param cp		The start of the string.
 * @param endp		Pointer to the end of the parsed string placed here.
 * @param base		The number base to use.
 *
 * @return		Converted value.
 */
long long int strtoll(const char *cp, char **endp, int base) {
	if(*cp == '-') {
		return -strtoull(cp + 1, endp, base);
	}
	return strtoull(cp, endp, base);
}
