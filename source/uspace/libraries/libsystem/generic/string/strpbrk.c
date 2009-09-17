/* strpbrk function
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
 * @brief		Find characters in string function.
 */

#include <string.h>

/** Find characters in a string
 *
 * Searches the given string for any of the specified characters.
 *
 * @param s		Pointer to the string to search in.
 * @param accept	Array of characters to search for.
 * 
 * @return		Pointer to character found, or NULL if none found.
 */
char *strpbrk(const char *s, const char *accept) {
	const char *c = NULL;

	if(!*s) {
		return NULL;
	}

	while(*s) {
		for(c = accept; *c; c++) {
			if(*s == *c) {
				break;
			}
		}
		if(*c) {
			break;
		}
		s++;
	}

	return (*c == 0) ? NULL : (char *)s;
}
