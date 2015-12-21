/*
 * Copyright (C) 2008-2010 Alex Smith
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
