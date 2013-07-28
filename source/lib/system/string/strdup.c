/*
 * Copyright (C) 2008-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Duplicate string function.
 */

#include <stdlib.h>
#include <string.h>

/**
 * Duplicate a string.
 *
 * Allocates a buffer big enough to hold the given string and copies the
 * string to it. The memory returned is allocated via malloc().
 *
 * @param src		Pointer to the source buffer.
 *
 * @return		Pointer to the allocated buffer containing the string.
 */
char *strdup(const char *s) {
	char *dup;
	size_t len = strlen(s) + 1;

	dup = malloc(len);
	if(!dup)
		return NULL;

	memcpy(dup, s, len);
	return dup;
}

/**
 * Duplicate a string with a length limit.
 *
 * Allocates a buffer either as big as the string or the maximum length
 * given, and then copies at most the number of bytes specified of the string
 * to it. If the string is longer than the limit, a null byte will be added
 * to the end of the duplicate. The memory returned should be freed with
 * kfree().
 *
 * @param src		Pointer to the source buffer.
 * @param n		Maximum number of bytes to copy.
 *
 * @return		Pointer to the allocated buffer containing the string.
 */
char *strndup(const char *s, size_t n) {
	size_t len;
	char *dup;

	len = strnlen(s, n);
	dup = malloc(len + 1);
	if(dup) {
		memcpy(dup, s, len);
		dup[len] = '\0';
	}

	return dup;
}
