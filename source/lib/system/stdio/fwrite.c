/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File write function.
 */

#include <unistd.h>

#include "stdio/stdio.h"

/**
 * Write to a file stream.
 *
 * Writes nmemb elements of data, each size bytes long, from a buffer into
 * a file stream.
 *
 * @param ptr           Buffer to write from.
 * @param size          Size of each element.
 * @param nmemb         Number of elements to write.
 * @param stream        Stream to write to.
 *
 * @return              Number of elements written successfully.
 */
size_t fwrite(const void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    ssize_t ret;

    if (!size || !nmemb)
        return 0;

    ret = write(stream->fd, ptr, size * nmemb);
    if (ret <= 0) {
        if (ret == 0) {
            stream->eof = true;
        } else {
            stream->err = true;
        }

        return 0;
    }

    return ret / size;
}
