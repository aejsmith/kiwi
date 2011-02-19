/*
 * Copyright (C) 2007-2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
