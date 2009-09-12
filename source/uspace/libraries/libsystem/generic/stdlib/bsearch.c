/* Array search function
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
 * @brief		Array search function.
 */

#include <stdlib.h>

/** Search a sorted array.
 *
 * Searches a sorted array of items for the given key.
 *
 * @param key		Key to search for.
 * @param base		Start of the array.
 * @param nmemb		Number of array elements.
 * @param size		Size of each array element.
 * @param compar	Comparison function.
 *
 * @return		Pointer to found key, or NULL if not found.
 */
void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	size_t low;
	size_t mid;
	char *p;
	int r;

	if(size > 0) {
		low = 0;
		while(low < nmemb) {
			mid = low + ((nmemb - low) >> 1);
			p = ((char *)base) + mid * size;
			r = (*compar)(key, p);
			if(r > 0) {
				low = mid + 1;
			} else if(r < 0) {
				nmemb = mid;
			} else {
				return p;
			}
		}
	}

	return NULL;
}
