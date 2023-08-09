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
 * @brief               Temporary file functions.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Letters to use for mkstemp(). */
static const char *mkstemp_letters =
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

/**
 * Create and open a temporary file.
 *
 * Creates and opens a new temporary file, with a name based on the given
 * template. The last 6 characters of the template must be XXXXXX, which
 * will be overwritten by the call.
 *
 * @param tpl           Template to base file name on.
 *
 * @return              File descriptor for file.
 */
int mkstemp(char *tpl) {
    size_t len, lcount, i, j;
    char *dest;
    int fd;

    len = strlen(tpl);
    if (len < 6) {
        errno = EINVAL;
        return -1;
    }

    dest = (tpl + len) - 6;
    if (strcmp(dest, "XXXXXX") != 0) {
        errno = EINVAL;
        return -1;
    }

    lcount = strlen(mkstemp_letters);
    for (i = 0; i < TMP_MAX; i++) {
        for (j = 0; j < 6; j++)
            dest[j] = mkstemp_letters[rand() % lcount];

        fd = open(tpl, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            return fd;
        } else if (errno != EEXIST) {
            return -1;
        }
    }

    errno = EEXIST;
    return -1;
}

/**
 * Create a temporary file.
 *
 * Creates new temporary file, with a name based on the given template. The
 * last 6 characters of the template must be XXXXXX, which will be overwritten
 * by the call. It is not recommended to use this function; use mkstemp()
 * instead.
 *
 * @param tpl           Template to base file name on.
 *
 * @return              Value supplied for tpl.
 */
char *mktemp(char *tpl) {
    int fd;

    fd = mkstemp(tpl);
    if (fd < 0) {
        tpl[0] = 0;
        return tpl;
    }

    close(fd);
    unlink(tpl);
    return tpl;
}
