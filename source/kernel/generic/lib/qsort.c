/* Kiwi array sort function
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
 * @brief		Array sort function.
 *
 * Reference:
 * - Quicksort is Optimal - R. Sedgewick
 *   http://www.cs.princeton.edu/~rs/talks/QuicksortIsOptimal.pdf
 */

#include <lib/qsort.h>

static void swap_items(char *base, size_t size, size_t idx1, size_t idx2) {
	char *item1 = base + (idx1 * size);
	char *item2 = base + (idx2 * size);
	char ch;
	size_t i;

	for(i = 0; i < size; i++) {
		ch = *item1;

		*item1 = *item2;
		*item2 = ch;

		item1++;
		item2++;
	}
}

/* I know this is twice as long as what's in the paper, but the code provided
 * is fucking unreadable. */
static void quicksort(char *base, size_t size, int l, int endidx, int (*compar)(const void *, const void *)) {
	int i = (l - 1), j = endidx, p = (l - 1), q = endidx, k;
	char *end = base + (endidx * size);

	if(endidx <= l) {
		return;
	}

	for (;;) {
		while(++i != endidx && compar(base + (i * size), end) < 0);
		while(compar(end, base + ((--j) * size)) < 0) {
			if(j == l) {
				break;
			}
		}

		if(i >= j) {
			break;
		}

		swap_items(base, size, i, j);

		if(compar(base + (i * size), end) == 0) {
			swap_items(base, size, ++p, i);
		}
		if(compar(end, base + (j * size)) == 0) {
			swap_items(base, size, j, --q);
		}
	}

	swap_items(base, size, i, endidx);
	j = (i - 1);
	i++;

	for(k = l; k < p; k++, j--) {
		swap_items(base, size, k, j);
	}

	for(k = (endidx - 1); k > q; k--, i++) {
		swap_items(base, size, i, k);
	}

	quicksort(base, size, l, j, compar);
	quicksort(base, size, i, endidx, compar);
}

/** Sort an array in ascending order.
 *
 * Sorts an array of items into ascending order, using the given function
 * to compare items.
 *
 * @param base		Start of the array.
 * @param nmemb		Number of array elements.
 * @param size		Size of each array element.
 * @param compar	Comparison function.
 */
void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
	if(nmemb > 1) {
		quicksort(base, size, 0, nmemb - 1, compar);
	}
}
