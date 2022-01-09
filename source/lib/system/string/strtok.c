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
 * @brief               String parsing functions.
 */

#include <string.h>

/**
 * Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str           String to parse (NULL to continue last string).
 * @param delim         Set of delimiters.
 * @param saveptr       Where to save state across calls.
 *
 * @return              Pointer to next token, or NULL if no more found.
 */
char *strtok_r(char *restrict str, const char *restrict delim, char **restrict saveptr) {
    char *ret = NULL;

    /* If string is NULL, continue with last operation. */
    if (!str)
        str = *saveptr;

    str += strspn(str, delim);
    if (*str) {
        ret = str;
        str += strcspn(str, delim);
        if (*str)
            *str++ = 0;
    }

    *saveptr = str;
    return ret;
}

/**
 * Parse a string into tokens.
 *
 * Parses a string into a sequence of tokens using the given delimiters.
 * The first call to this function should specify the string to parse
 * in str, subsequent calls operating on the same string should pass NULL
 * for str.
 *
 * @param str           String to parse (NULL to continue last string).
 * @param delim         Set of delimiters.
 *
 * @return              Pointer to next token, or NULL if no more found.
 */
char *strtok(char *restrict str, const char *restrict delim) {
    static char *strtok_state = NULL;

    return strtok_r(str, delim, &strtok_state);
}
