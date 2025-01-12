/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
