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
 * @brief               Case-insensitive string comparison functions.
 */

#include <ctype.h>
#include <stdbool.h>
#include <string.h>

/** Compare two strings ignorning case.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strcasecmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    while (true) {
        c1 = tolower(*s1++);
        c2 = tolower(*s2++);

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;
    }
}

/** Compare two strings with a length limit ignoring case.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @param count         Maximum number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strncasecmp(const char *s1, const char *s2, size_t count) {
    unsigned char c1, c2;

    while (count) {
        c1 = tolower(*s1++);
        c2 = tolower(*s2++);

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;

        count--;
    }

    return 0;
}
