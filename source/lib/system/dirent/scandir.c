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
 * @brief               Scan directory function.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "dirent/dirent.h"

/**
 * Get an array of directory entries.
 *
 * Gets an array of directory entries from a directory, filters them and
 * sorts them using the given functions.
 *
 * @param path          Path to directory.
 * @param namelist      Where to store array pointer.
 * @param filter        Function to filter entries (should return zero if
 *                      an entry should be ignored).
 * @param compar        Comparison function.
 *
 * @return              Number of entries.
 */
int scandir(
    const char *path, struct dirent ***namelist,
    int (*filter)(const struct dirent *),
    int (*compar)(const void *, const void *))
{
    struct dirent **list = NULL, **nlist, *dent;
    int count = 0, i;
    DIR *dir;

    dir = opendir(path);
    if (!dir)
        return -1;

    /* Clear errno so we can detect failures. */
    errno = 0;

    /* Loop through all directory entries. */
    while ((dent = readdir(dir))) {
        if (filter && !filter(dent))
            continue;

        nlist = realloc(list, sizeof(void *) * (count + 1));
        if (!nlist) {
            break;
        } else {
            list = nlist;
        }

        /* Insert it into the list. */
        list[count] = malloc(dent->d_reclen);
        if (!list[count])
            break;

        memcpy(list[count], dent, dent->d_reclen);
        count++;
    }

    if (errno != 0) {
        for (i = 0; i < count; i++)
            free(list[i]);

        free(list);
        closedir(dir);
        return -1;
    }

    closedir(dir);

    if (compar)
        qsort(list, count, sizeof(void *), compar);

    *namelist = list;
    return count;
}
