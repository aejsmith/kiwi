/* Kiwi C library - strcat function
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
 * @brief		String concatenate function.
 */

#include <string.h>

/** Concatenate 2 strings.
 *
 * Appends one string to another.
 *
 * @param dest		Pointer to the string to append to.
 * @param src		Pointer to the string to append.
 * 
 * @return		Pointer to dest.
 */
char *strcat(char *dest, const char *src) {
	size_t destlen = strlen(dest);
	char *d = dest + destlen;

	while((*d++ = *src++));
	return dest;
}

/** Concatenate 2 strings.
 *
 * Appends one string to another, with a maximum limit on how much of the
 * source string to copy.
 *
 * @param dest		Pointer to the string to append to.
 * @param src		Pointer to the string to append.
 * @param max		Maximum number of characters to use from src.
 * 
 * @return		Pointer to dest.
 */
char *strncat(char *dest, const char *src, size_t max) {
	size_t i, destlen = strlen(dest);
	char *d = dest + destlen;

	for(i = 0; i < max && src[i] != 0; i++) {
		d[i] = src[i];
	}

	d[i] = 0;
	return dest;
}
