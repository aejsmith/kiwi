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
