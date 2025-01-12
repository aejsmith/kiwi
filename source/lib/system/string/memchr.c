/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Memory searching functions.
 */

#include <string.h>

/** Find the first occurrence of a character in an area of memory.
 * @param s             Pointer to the memory to search.
 * @param c             Character to search for.
 * @param n             Size of memory.
 * @return              NULL if token not found, otherwise pointer to token. */
void *memchr(const void *s, int c, size_t n) {
    const unsigned char *src = (const unsigned char *)s;
    unsigned char ch = c;

    for (; n != 0; n--) {
        if (*src == ch)
            return (void *)src;

        src++;
    }

    return NULL;
}
