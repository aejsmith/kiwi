/*
 * Copyright (C) 2007-2013 Alex Smith
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
 * @brief		Memory comparison function.
 */

#include <string.h>

/** Compare 2 chunks of memory.
 * @param p1		Pointer to the first chunk.
 * @param p2		Pointer to the second chunk.
 * @param count		Number of bytes to compare.
 * @return		An integer less than, equal to or greater than 0 if
 *			p1 is found, respectively, to be less than, to match,
 *			or to be greater than p2. */
int memcmp(const void *p1, const void *p2, size_t count) {
	const unsigned char *s1 = (const unsigned char *)p1;
	const unsigned char *s2 = (const unsigned char *)p2;

	while(count--) {
		if(*s1 != *s2)
			return *s1 - *s2;

		s1++;
		s2++;
	}

	return 0;
}
