/* strcpy/strncpy functions
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
 * @brief		String copying functions.
 */

#include <string.h>

/** Copy a string.
 *
 * Copies a string from one place to another. Assumes that the destination
 * is big enough to hold the string.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * 
 * @return		The value specified for dest.
 */
char *strcpy(char *dest, const char *src) {
	char *d = dest;

	while((*d++ = *src++));
	return dest;
}

/** Copy a string with a length limit.
 *
 * Copies a string from one place to another. Will copy at most the number
 * of bytes specified.
 *
 * @param dest		Pointer to the destination buffer.
 * @param src		Pointer to the source buffer.
 * @param count		Maximum number of bytes to copy.
 * 
 * @return		The value specified for dest.
 */
char *strncpy(char *dest, const char *src, size_t count) {
	size_t i;
	
	for(i = 0; i < count; i++) {
		dest[i] = src[i];
		if(!src[i]) {
			break;
		}
	}
	return dest;
}
