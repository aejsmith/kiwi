/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
