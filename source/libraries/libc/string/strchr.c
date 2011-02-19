/*
 * Copyright (C) 2007-2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
