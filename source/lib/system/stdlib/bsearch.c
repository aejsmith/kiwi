/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Array search function.
 */

#include <stdlib.h>

/** Search a sorted array.
 * @param key           Key to search for.
 * @param base          Start of the array.
 * @param nmemb         Number of array elements.
 * @param size          Size of each array element.
 * @param compar        Comparison function.
 * @return              Pointer to found key, or NULL if not found. */
void *bsearch(
    const void *key, const void *base, size_t nmemb, size_t size,
    int (*compar)(const void *, const void *))
{
    size_t low;
    size_t mid;
    char *p;
    int r;

    if (size > 0) {
        low = 0;
        while (low < nmemb) {
            mid = low + ((nmemb - low) >> 1);
            p = ((char *)base) + mid * size;
            r = (*compar)(key, p);
            if (r > 0) {
                low = mid + 1;
            } else if (r < 0) {
                nmemb = mid;
            } else {
                return p;
            }
        }
    }

    return NULL;
}
