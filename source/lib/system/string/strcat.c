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
 * @brief               String concatenate function.
 */

#include <string.h>

/**
 * Concatenate two strings.
 *
 * Appends one string to another. Assumes that the destination string has
 * enough space to store the contents of both strings and the NULL terminator.
 *
 * @param dest          Pointer to the string to append to.
 * @param src           Pointer to the string to append.
 *
 * @return              Pointer to dest.
 */
char *strcat(char *__restrict dest, const char *__restrict src) {
    size_t len = strlen(dest);
    char *d = dest + len;

    while ((*d++ = *src++))
        ;

    return dest;
}

/**
 * Concatenate 2 strings.
 *
 * Appends one string to another, with a maximum limit on how much of the
 * source string to copy.
 *
 * @param dest          Pointer to the string to append to.
 * @param src           Pointer to the string to append.
 * @param max           Maximum number of characters to use from src.
 * 
 * @return              Pointer to dest.
 */
char *strncat(char *restrict dest, const char *restrict src, size_t max) {
    size_t i, destlen = strlen(dest);
    char *d = dest + destlen;

    for (i = 0; i < max && src[i] != 0; i++)
        d[i] = src[i];

    d[i] = 0;
    return dest;
}
