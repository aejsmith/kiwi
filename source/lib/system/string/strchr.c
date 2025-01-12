/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String searching functions.
 */

#include <stdbool.h>
#include <string.h>

/** Find first occurrence of a character in a string.
 * @param s             Pointer to the string to search.
 * @param c             Character to search for.
 * @return              NULL if token not found, otherwise pointer to token. */
char *strchr(const char *s, int c) {
    char ch = c;

    while (true) {
        if (*s == ch) {
            break;
        } else if (!*s) {
            return NULL;
        } else {
            s++;
        }
    }

    return (char *)s;
}

/** Find last occurrence of a character in a string.
 * @param s             Pointer to the string to search.
 * @param c             Character to search for.
 * @return              NULL if token not found, otherwise pointer to token. */
char *strrchr(const char *s, int c) {
    const char *l = NULL;

    while (true) {
        if (*s == c)
            l = s;
        if (!*s)
            return (char *)l;
        s++;
    }

    return (char *)l;
}
