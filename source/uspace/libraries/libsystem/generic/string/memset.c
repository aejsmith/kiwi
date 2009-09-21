/* memset function
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
 * @brief		Memory setting function.
 */

#include <stddef.h>
#include <string.h>

/** Fill a memory area.
 *
 * Fills a memory area with the value specified.
 *
 * @param dest		The memory area to fill.
 * @param val		The value to fill with (converted to an unsigned char).
 * @param count		The number of bytes to fill.
 *
 * @return		Destination location.
 */
void *memset(void *dest, int val, size_t count) {
	unsigned char c = val & 0xff;
	unsigned long *nd, nval;
	char *d = (char *)dest;
	size_t i;

	/* Align the destination. */
	while((ptrdiff_t)d & (sizeof(unsigned long) - 1)) {
		if(count--) {
			*d++ = c;
		} else {
			return dest;
		}
	}

	/* Write in native-sized blocks if we can. */
	if(count >= sizeof(unsigned long)) {
		nd = (unsigned long *)d;

		/* Compute the value we will write. */
		nval = c;
		if(nval != 0) {
			for(i = 8; i < (sizeof(unsigned long) * 8); i <<= 1) {
				nval = (nval << i) | nval;
			}
		}

		/* Unroll the loop if possible. */
		while(count >= (sizeof(unsigned long) * 4)) {
			*nd++ = nval;
			*nd++ = nval;
			*nd++ = nval;
			*nd++ = nval;
			count -= sizeof(unsigned long) * 4;
		}
		while(count >= sizeof(unsigned long)) {
			*nd++ = nval;
			count -= sizeof(unsigned long);
		}

		d = (char *)nd;
	}

	/* Write remaining bytes. */
	while(count--) {
		*d++ = val;
	}
	return dest;
}
