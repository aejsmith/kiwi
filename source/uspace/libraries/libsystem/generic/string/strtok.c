/*
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
 * @brief		String parsing functions.
 */

#include <string.h>

/** Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str		String to parse (NULL to continue last string).
 * @param delim		Set of delimiters.
 * @param saveptr	Where to save state across calls.
 *
 * @return		Pointer to next token, or NULL if no more found.
 */
char *strtok_r(char *str, const char *delim, char **saveptr) {
	char *ret = NULL;

	/* If string is NULL, continue with last operation. */
	if(str == NULL) {
		str = *saveptr;
	}

	str += strspn(str, delim);
	if(*str) {
		ret = str;
		str += strcspn(str, delim);
		if(*str) {
			*str++ = 0;
		}
	}

	*saveptr = str;
	return ret;
}

/** Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str		String to parse (NULL to continue last string).
 * @param delim		Set of delimiters.
 *
 * @return		Pointer to next token, or NULL if no more found.
 */
char *strtok(char *str, const char *delim) {
	static char *strtok_state = NULL;

	return strtok_r(str, delim, &strtok_state);
}
