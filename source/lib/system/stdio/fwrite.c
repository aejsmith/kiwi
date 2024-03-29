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
