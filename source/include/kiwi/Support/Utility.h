/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Utility functions.
 */

#ifndef __KIWI_SUPPORT_UTILITY_H
#define __KIWI_SUPPORT_UTILITY_H

#include <kiwi/CoreDefs.h>

namespace kiwi {

/** Rounds a value up to a power of two.
 * @param n		Value to round up.
 * @param align		Value to round up to. */
template <typename T>
T p2align(T n, int align) {
	if(n & (align - 1)) {
		n += align;
		n &= ~(align - 1);
	}
	return n;
}

/** Get the size of an array.
 * @param array		Array to get size of. */
template <typename T, size_t N>
size_t array_size(T (&array)[N]) {
	return N;
}

}

#endif /* __KIWI_SUPPORT_UTILITY_H */
