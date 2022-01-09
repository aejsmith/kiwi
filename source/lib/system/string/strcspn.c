/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               String searching functions.
 */

#include <string.h>

/**
 * Find the index of the first character in the given set.
 *
 * Returns the index of the first character in the given string which is in
 * the the supplied set of characters.
 *
 * @param s             String to span.
 * @param reject        Characters to reject.
 *
 * @return              Index of character.
 */
size_t strcspn(const char *s, const char *reject) {
    size_t count = 0;
    int i, j;

    for (i = 0; s[i]; i++) {
        for (j = 0; reject[j]; j++) {
            if (s[i] == reject[j])
                return count;
        }

        count++;
    }

    return count;
}

/**
 * Find the index of the first character not in the given set.
 *
 * Returns the index of the first character in the given string which is not in
 * the the supplied set of characters.
 *
 * @param s             String to span.
 * @param accept        Characters to accept.
 *
 * @return              Index of character.
 */
size_t strspn(const char *s, const char *accept) {
    size_t count = 0;
    const char *a;
    int i;

    for (i = 0; s[i]; i++) {
        for (a = accept; *a && s[i] != *a; a++)
            ;

        if (!*a)
            break;

        count++;
    }

    return count;
}
