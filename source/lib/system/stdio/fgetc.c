/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Get character functions.
 */

#include <unistd.h>
#include "stdio/stdio.h"

/** Read character from a stream.
 * @param stream        Stream to read from.
 * @return              Chararcter read or EOF on failure. */
int fgetc(FILE *stream) {
    unsigned char ch;
    ssize_t ret;

    if (stream->have_pushback) {
        stream->have_pushback = false;
        return stream->pushback_ch;
    }

    ret = read(stream->fd, &ch, 1);
    if (ret < 0) {
        stream->err = 1;
        return EOF;
    } else if (ret < 1) {
        stream->eof = 1;
        return EOF;
    }

    return (int)ch;
}

/** Read character from a stream.
 * @param stream        Stream to read from.
 * @return              Chararcter read or EOF on failure. */
int getc(FILE *stream) {
    return fgetc(stream);
}

/** Read character from standard input.
 * @return              Chararcter read or EOF on failure. */
int getchar(void) {
    return fgetc(stdin);
}

/**
 * Push a character back to a stream.
 *
 * Pushes the given character back onto the given input stream, to be read
 * by the next call to fgetc() or fread(). Only one character is stored: this
 * function will overwrite any existing pushed-back character.
 *
 * @param ch            Character to push.
 * @param stream        Stream to push to.
 *
 * @return              Character pushed.
 */
int ungetc(int ch, FILE *stream) {
    stream->pushback_ch = ch;
    stream->have_pushback = true;
    stream->eof = false;
    return ch;
}
