/* memmove function
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
 * @brief		Memory moving function.
 */

#include <string.h>

/** Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memmove(void *dest, const void *src, size_t count) {
	const char *b = src;
	char *a = dest;

	if(src != dest) {
		if(src > dest) {
			memcpy(dest, src, count);
		} else {
			a += count - 1;
			b += count - 1;
			while(count--) {
				*a-- = *b--;
			}
		}
	}

	return dest;
}
