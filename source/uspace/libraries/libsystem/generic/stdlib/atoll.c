/* String to integer functions
 * Copyright (C) 2008-2009 Alex Smith
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

#include <stdlib.h>

/** Convert string to long long.
 *
 * Converts the initial part of a string to a long long.
 *
 * @param s		String to convert.
 *
 * @return		Converted value.
 */
long long atoll(const char *s) {
	return strtoll(s, NULL, 10);
}

/** Convert string to long.
 *
 * Converts the initial part of a string to a long.
 *
 * @param s		String to convert.
 *
 * @return		Converted value.
 */
long atol(const char *s) {
	return strtol(s, NULL, 10);
}
