/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String searching functions.
 */

#include <string.h>

/** Find the first occurrence of a substring in a string.
 * @param s             String to search.
 * @param what          Substring to search for.
 * @return              Pointer to start of match if found, null if not. */
char *strstr(const char *s, const char *what) {
    size_t len = strlen(what);

    while (*s) {
        if (strncmp(s, what, len) == 0)
            return (char *)s;
        s++;
    }

    return NULL;
}

