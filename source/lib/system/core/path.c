/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Path manipulation functions.
 */

#include <core/path.h>

#include <stdlib.h>
#include <string.h>

/**
 * Get the last component of a path.
 *
 * Returns an allocated string buffer containing the last component of the
 * given path.
 *
 * @param path          Pathname to parse.
 *
 * @return              Pointer to string containing last component of path, or
 *                      null on failure. The string returned is allocated via
 *                      malloc(), so should be freed using free().
 */
char *core_path_basename(const char *path) {
    char *ptr, *dup, *ret;
    size_t len;

    if (!path || !path[0] || (path[0] == '.' && !path[1])) {
        return strdup(".");
    } else if (path[0] == '.' && path[1] == '.' && !path[2]) {
        return strdup("..");
    }

    dup = strdup(path);
    if (!dup)
        return NULL;

    /* Strip off trailing '/' characters. */
    len = strlen(dup);
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    /* If length is now 0, the entire string was '/' characters. */
    if (!len) {
        free(dup);
        return strdup("/");
    }

    ptr = strrchr(dup, '/');
    if (!ptr) {
        /* No '/' character in the string, that means what we have is correct
         * correct. Resize the allocation to the new length. */
        ret = realloc(dup, len + 1);
        if (!ret)
            free(dup);

        return ret;
    } else {
        ret = strdup(ptr + 1);
        free(dup);
        return ret;
    }
}

/**
 * Get the part of a path preceding the last /.
 *
 * Returns an allocated string buffer containing everything preceding the last
 * component of the given path.
 *
 * @param path          Pathname to parse.
 *
 * @return              Pointer to string, or null on failure. The string
 *                      returned is allocated via malloc(), so should be freed
 *                      using free().
 */
char *core_path_dirname(const char *path) {
    char *ptr, *dup, *ret;
    size_t len;

    if (!path || !path[0] || (path[0] == '.' && !path[1])) {
        return strdup(".");
    } else if (path[0] == '.' && path[1] == '.' && !path[2]) {
        return strdup(".");
    }

    /* Duplicate string to modify it. */
    dup = strdup(path);
    if (!dup)
        return NULL;

    /* Strip off trailing '/' characters. */
    len = strlen(dup);
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    /* Look for last '/' character. */
    ptr = strrchr(dup, '/');
    if (!ptr) {
        free(dup);
        return strdup(".");
    }

    /* Strip off the character and any extras. */
    len = (ptr - dup) + 1;
    while (len && dup[len - 1] == '/')
        dup[--len] = 0;

    if (!len) {
        free(dup);
        return strdup("/");
    }

    ret = realloc(dup, len + 1);
    if (!ret)
        free(dup);

    return ret;
}
