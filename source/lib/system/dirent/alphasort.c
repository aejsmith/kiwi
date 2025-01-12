/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Alphabetical sort function.
 */

#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/**
 * Sort directory entries in alphabetical order.
 *
 * Sort function to be used with scandir() to sort entries in alphabetical
 * order.
 *
 * @param a         Pointer to pointer to first entry.
 * @param b         Pointer to pointer to second entry.
 *
 * @return          An integer less than, equal to or greater than 0 if
 *                  a is found, respectively, to be less than, to match,
 *                  or to be greater than b.
 */
int alphasort(const void *a, const void *b) {
    const struct dirent *d1 = *(const struct dirent **)a;
    const struct dirent *d2 = *(const struct dirent **)b;

    return strcmp(d1->d_name, d2->d_name);
}
