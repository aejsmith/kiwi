/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
        stream->err = true;
        return EOF;
    } else if (ret < 1) {
        stream->eof = true;
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
