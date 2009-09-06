/* strchr/strrchr functions
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
 * @brief		String searching functions.
 */

#include <string.h>

/** Find a character in a string.
 *
 * Finds the first occurrence of a character in the specified string.
 *
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * 
 * @return		NULL if token not found, otherwise pointer to token.
 */
char *strchr(const char *s, int c) {
	char ch = c;

	for (;;) {
		if(*s == ch)
			break;
		else if(!*s)
			return NULL;
		else
			s++;
	}

	return (char *)s;
}

/** Find a character in a string.
 *
 * Finds the last occurrence of a character in the specified string.
 *
 * @param s		Pointer to the string to search.
 * @param c		Character to search for.
 * 
 * @return		NULL if token not found, otherwise pointer to token.
 */
char *strrchr(const char *s, int c) {
	const char *l = NULL;

	for(;;) {
		if(*s == c)
			l = s;
		if (!*s)
			return (char *)l;
		s++;
	}

	return (char *)l;
}
