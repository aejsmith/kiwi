/*
 * Copyright (C) 2010 Alex Smith
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
