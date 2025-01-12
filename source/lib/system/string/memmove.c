/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory moving function.
 */

#include <string.h>

/**
 * Copy overlapping data in memory.
 *
 * Copies bytes from a source memory area to a destination memory area,
 * where both areas may overlap.
 *
 * @param dest          The memory area to copy to.
 * @param src           The memory area to copy from.
 * @param count         The number of bytes to copy.
 *
 * @return              Destination location.
 */
void *memmove(void *dest, const void *src, size_t count) {
    const unsigned char *s;
    unsigned char *d;

    if (src != dest) {
        if (src > dest) {
            memcpy(dest, src, count);
        } else {
            d = (unsigned char *)dest + (count - 1);
            s = (const unsigned char *)src + (count - 1);
            while (count--)
                *d-- = *s--;
        }
    }

    return dest;
}
