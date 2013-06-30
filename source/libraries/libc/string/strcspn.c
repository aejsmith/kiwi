/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		String searching functions.
 */

#include <string.h>

/** Count characters not in a string.
 *
 * Counts the length of the initial part of the given string which consists
 * of characters not from the given disallowed characters, excluding the
 * terminating NULL byte.
 *
 * @param s		String to count in.
 * @param reject	Characters to reject.
 *
 * @return		Count of characters.
 */
size_t strcspn(const char *s, const char *reject) {
	size_t count = 0;
	int i, j;

	for(i = 0; s[i]; i++) {
		for(j = 0; reject[j]; j++) {
			if(s[i] == reject[j]) {
				return count;
			}
		}
		count++;
	}

	return count;
}

/** Get length of segment of string.
 *
 * Gets the length of the initial part of the given string which consists
 * of characters from the given allowed characters, excluding the terminating
 * NULL byte.
 *
 * @param s		String to count in.
 * @param accept	Characters to accept.
 *
 * @return		Count of characters.
 */
size_t strspn(const char *s, const char *accept) {
	size_t count = 0;
	const char *a;
	int i;

	for(i = 0; s[i]; i++) {
		for(a = accept; *a && s[i] != *a; a++);
		if(!*a) {
			break;
		}
		count++;
	}
	return count;
}
