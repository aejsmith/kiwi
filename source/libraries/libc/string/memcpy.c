/*
 * Copyright (C) 2007-2010 Alex Smith
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
 * @brief		Memory copying function.
 */

#include <stddef.h>
#include <string.h>

/** Copy data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may not overlap.
 *
 * @note		This function does not like unaligned addresses. Giving
 *			it unaligned addresses might make it sad. :(
 *
 * @param dest		The memory area to copy to.
 * @param src		The memory area to copy from.
 * @param count		The number of bytes to copy.
 *
 * @return		Destination location.
 */
void *memcpy(void *restrict dest, const void *restrict src, size_t count) {
	const char *s = (const char *)src;
	char *d = (char *)dest;
	const unsigned long *ns;
	unsigned long *nd;

	/* Align the destination. */
	while((ptrdiff_t)d & (sizeof(unsigned long) - 1)) {
		if(count--) {
			*d++ = *s++;
		} else {
			return dest;
		}
	}

	/* Write in native-sized blocks if we can. */
	if(count >= sizeof(unsigned long)) {
		nd = (unsigned long *)d;
		ns = (const unsigned long *)s;

		/* Unroll the loop if possible. */
		while(count >= (sizeof(unsigned long) * 4)) {
			*nd++ = *ns++;
			*nd++ = *ns++;
			*nd++ = *ns++;
			*nd++ = *ns++;
			count -= sizeof(unsigned long) * 4;
		}
		while(count >= sizeof(unsigned long)) {
			*nd++ = *ns++;
			count -= sizeof(unsigned long);
		}

		d = (char *)nd;
		s = (const char *)ns;
	}

	/* Write remaining bytes. */
	while(count--) {
		*d++ = *s++;
	}
	return dest;
}
