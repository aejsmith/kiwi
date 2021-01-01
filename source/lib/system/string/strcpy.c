/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               String copying functions.
 */

#include <string.h>

/**
 * Copy a string.
 *
 * Copies a string from one place to another. Assumes that the destination
 * is big enough to hold the string.
 *
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 *
 * @return              The value specified for dest.
 */
char *strcpy(char *__restrict dest, const char *__restrict src) {
    char *d = dest;

    while ((*d++ = *src++))
        ;

    return dest;
}

/**
 * Copy a string with a length limit.
 *
 * Copies a string from one place to another. Will copy at most the number
 * of bytes specified.
 *
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 * @param count         Maximum number of bytes to copy.
 *
 * @return              The value specified for dest.
 */
char *strncpy(char *__restrict dest, const char *__restrict src, size_t count) {
    size_t i;

    for (i = 0; i < count; i++) {
        dest[i] = src[i];
        if (!src[i])
            break;
    }

    return dest;
}

/**
 * Transform a string.
 *
 * Transforms a string so that the result of strcmp() on the transformed string
 * is the same as the result of strcoll() on the string.
 *
 * @param dest          Pointer to the destination buffer.
 * @param src           Pointer to the source buffer.
 * @param count         Size of destination buffer in bytes.
 * 
 * @return              Number of bytes required in dest excluding NULL
 *                      terminator.
 */
size_t strxfrm(char *restrict dest, const char *restrict src, size_t count) {
    size_t i;

    // TODO: Proper implementation.
    for (i = 0; i < count; i++) {
        dest[i] = src[i];
        if (!src[i])
            break;
    }

    return i;
}
