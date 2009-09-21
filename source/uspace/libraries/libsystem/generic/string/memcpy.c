/* memcpy function
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
void *memcpy(void *dest, const void *src, size_t count) {
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
