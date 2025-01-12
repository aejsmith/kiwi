/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
