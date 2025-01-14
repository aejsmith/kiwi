/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory comparison function.
 */

#include <string.h>

/** Compare 2 chunks of memory.
 * @param p1            Pointer to the first chunk.
 * @param p2            Pointer to the second chunk.
 * @param count         Number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      p1 is found, respectively, to be less than, to match,
 *                      or to be greater than p2. */
int memcmp(const void *p1, const void *p2, size_t count) {
    const unsigned char *s1 = (const unsigned char *)p1;
    const unsigned char *s2 = (const unsigned char *)p2;

    while (count--) {
        if (*s1 != *s2)
            return *s1 - *s2;

        s1++;
        s2++;
    }

    return 0;
}
