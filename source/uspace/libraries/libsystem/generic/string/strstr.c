/* strstr function
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
 * @brief		String searching functions.
 */

#include <string.h>

/** Find a string within a string.
 *
 * Finds the first occurrence of a string within the specified string.
 *
 * @param haystack	Pointer to the string to search.
 * @param needle	String to search for.
 * 
 * @return		NULL if token not found, otherwise pointer to string.
 */
char *strstr(const char *haystack, const char *needle) {
	while(*haystack) {
		if(strncmp(haystack, needle, strlen(needle)) == 0) {
			return (char *)haystack;
		}
		haystack++;
	}

	return NULL;
}
