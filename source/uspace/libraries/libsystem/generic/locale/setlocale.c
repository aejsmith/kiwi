/* Set locale function
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
 * @brief		Set locale function.
 */

#include <locale.h>
#include <string.h>

/** Set the current locale.
 *
 * Sets the current locale for the given category to the locale corresponding
 * to the given string.
 *
 * @param category	Category to set locale for.
 * @param name		Name of locale to set.
 *
 * @return		Name of new locale.
 */
char *setlocale(int category, const char *name) {
	if(name != NULL) {
		if(strcmp(name, "C") && strcmp(name, "POSIX") && strcmp(name, "")) {
			return NULL;
		}
	}
	return (char *)"C";
}
