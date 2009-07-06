/* Kiwi C library - Duplicate string function.
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
 * @brief		Duplicate string function.
 */

#include <stdlib.h>
#include <string.h>

/** Duplicate a string.
 *
 * Allocates a buffer big enough to hold the given string and copies the
 * string to it. The pointer returned should be freed with free().
 *
 * @param s		Pointer to the source buffer.
 * 
 * @return		Pointer to the allocated buffer containing the string.
 */
char *strdup(const char *s) {
	char *dup;
	size_t len = strlen(s) + 1;

	dup = malloc(len);
	if(dup == NULL) {
		return NULL;
	}

	memcpy(dup, s, len);
	return dup;
}

/** Duplicate a string with a length limit.
 *
 * Allocates a buffer either as big as the string or the maximum length
 * given, and then copies at most the number of bytes specified of the string
 * to it. If the string is longer than the limit, a null byte will be added
 * to the end of the duplicate. The pointer returned should be freed with
 * free().
 *
 * @param s		Pointer to the source buffer.
 * @param n		Maximum number of bytes to copy.
 * 
 * @return		Pointer to the allocated buffer containing the string.
 */
char *strndup(const char *s, size_t n) {
	size_t len;
	char *dup;

	len = strlen(s);
	if(n < len) {
		len = n;
	}

	dup = malloc(len + 1);
	if(dup == NULL) {
		return NULL;
	}

	memcpy(dup, s, len);
	dup[len] = '\0';
	return dup;
}
