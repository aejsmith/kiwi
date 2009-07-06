/* Kiwi C library - memchr function
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
 * @brief		Memory searching functions.
 */

#include <string.h>

/** Find a character in an area of memory.
 *
 * Finds the first occurrence of a character in an area of memory.
 *
 * @param s		Pointer to the memory to search.
 * @param c		Character to search for.
 * @param n		Size of memory.
 * 
 * @return		NULL if token not found, otherwise pointer to token.
 */
void *memchr(const void *s, int c, size_t n) {
	const unsigned char *src = (const unsigned char *)s;
	unsigned char ch = c;

	for (; n != 0; n--) {
		if(*src == ch) {
			return (void *)src;
		}
		src++;
	}

	return NULL;
}
