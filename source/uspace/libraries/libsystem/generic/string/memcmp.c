/* memcmp function
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
 * @brief		Memory comparison function.
 */

#include <string.h>

/** Compare 2 chunks of memory.
 *
 * Compares the two chunks of memory specified.
 *
 * @param p1		Pointer to the first chunk.
 * @param p2		Pointer to the second chunk.
 * @param count		Number of bytes to compare.
 * 
 * @return		An integer less than, equal to or greater than 0 if
 *			p1 is found, respectively, to be less than, to match,
 *			or to be greater than p2.
 */
int memcmp(const void *p1, const void *p2, size_t count) {
	unsigned char *s1 = (unsigned char *)p1;
	unsigned char *s2 = (unsigned char *)p2;

	while(count--) {
		if(*s1 != *s2) {
			return *s1 - *s2;
		}
		s1++;
		s2++;
	}

	return 0;
}
