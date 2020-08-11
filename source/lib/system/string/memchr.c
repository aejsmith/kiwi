/*
 * Copyright (C) 2009-2020 Alex Smith
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
