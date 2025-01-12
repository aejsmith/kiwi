/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String comparison functions.
 */

#include <stdbool.h>
#include <string.h>

/** Compare two strings.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strcmp(const char *s1, const char *s2) {
    unsigned char c1, c2;

    while (true) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;
    }
}

/** Compare two strings with a length limit.
 * @param s1            Pointer to the first string.
 * @param s2            Pointer to the second string.
 * @param count         Maximum number of bytes to compare.
 * @return              An integer less than, equal to or greater than 0 if
 *                      s1 is found, respectively, to be less than, to match,
 *                      or to be greater than s2. */
int strncmp(const char *s1, const char *s2, size_t count) {
    unsigned char c1, c2;

    while (count) {
        c1 = *s1++;
        c2 = *s2++;

        if (c1 != c2 || !c1)
            return (int)c1 - (int)c2;

        count--;
    }

    return 0;
}

int strcoll(const char *s1, const char *s2) {
    return strcmp(s1, s2);
}
