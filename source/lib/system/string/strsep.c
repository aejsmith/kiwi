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
 * @brief               String separating function.
 */

#include <string.h>

/**
 * Separate a string.
 *
 * Finds the first occurrence of a symbol in the string delim in *stringp.
 * If one is found, the delimeter is replaced by a NULL byte and the pointer
 * pointed to by stringp is updated to point past the string. If no delimeter
 * is found *stringp is made NULL and the token is taken to be the entire
 * string.
 *
 * @param stringp       Pointer to a pointer to the string to separate.
 * @param delim         String containing all possible delimeters.
 *
 * @return              NULL if stringp is NULL, otherwise a pointer to the
 *                      token found.
 */
char *strsep(char **stringp, const char *delim) {
    char *s;
    const char *spanp;
    int c, sc;
    char *tok;

    s = *stringp;
    if (!s)
        return NULL;

    for (tok = s;;) {
        c = *s++;
        spanp = delim;
        do {
            sc = *spanp++;
            if (sc == c) {
                if (c == 0) {
                    s = NULL;
                } else {
                    s[-1] = 0;
                }

                *stringp = s;
                return tok;
            }
        } while (sc != 0);
    }
}
