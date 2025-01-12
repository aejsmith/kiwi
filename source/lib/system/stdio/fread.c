/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File read function.
 */

#include <unistd.h>

#include "stdio/stdio.h"

/**
 * Read from a file stream.
 *
 * Reads nmemb elements of data, each size bytes long, from a file stream
 * into a buffer.
 *
 * @param ptr           Buffer to read into.
 * @param size          Size of each element.
 * @param nmemb         Number of elements to read.
 * @param stream        Stream to read from.
 *
 * @return              Number of elements read successfully.
 */
size_t fread(void *restrict ptr, size_t size, size_t nmemb, FILE *restrict stream) {
    char *buf = (char *)ptr;
    size_t total, count = 0;
    ssize_t ret;

    total = size * nmemb;
    if (!total)
        return 0;

    /* Read the pushed back character if there is one. */
    if (stream->have_pushback) {
        buf[count++] = stream->pushback_ch;
        stream->have_pushback = false;
    }

    /* Read remaining data. */
    if (count < total) {
        ret = read(stream->fd, &buf[count], total - count);
        if (ret > 0) {
            count += ret;
        } else if (ret == 0) {
            stream->eof = true;
        } else {
            stream->err = true;
        }
    }

    return count / size;
}
