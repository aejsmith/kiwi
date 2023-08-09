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
 * @brief               Put character functions.
 */

#include <unistd.h>

#include "stdio/stdio.h"

/** Write a character to a stream.
 * @param ch            Character to write.
 * @param stream        Stream to write to.
 * @return              EOF on failure, character written on success. */
int fputc(int ch, FILE *stream) {
    unsigned char val = (unsigned char)ch;
    ssize_t ret;

    ret = write(stream->fd, &val, 1);
    if (ret < 0) {
        stream->err = true;
        return EOF;
    } else if (ret < 1) {
        stream->eof = true;
        return EOF;
    }

    return (int)val;
}

/** Write character to a stream.
 * @param ch            Character to write.
 * @param stream        Stream to write to.
 * @return              EOF on failure, character written on success. */
int putc(int ch, FILE *stream) {
    return fputc(ch, stream);
}

/** Write character to standard output.
 * @param ch            Character to write.
 * @return              EOF on failure, character written on success. */
int putchar(int ch) {
    return fputc(ch, stdout);
}
