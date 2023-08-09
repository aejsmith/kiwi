/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Find characters in string function.
 */

#include <string.h>

/** Find characters in a string.
 * @param s             Pointer to the string to search in.
 * @param accept        Array of characters to search for.
 * @return              Pointer to character found, or NULL if none found. */
char *strpbrk(const char *s, const char *accept) {
    const char *c = NULL;

    if (!*s)
        return NULL;

    while (*s) {
        for (c = accept; *c; c++) {
            if (*s == *c)
                break;
        }

        if (*c)
            break;

        s++;
    }

    return (!*c) ? NULL : (char *)s;
}
