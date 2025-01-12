/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
