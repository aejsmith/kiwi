/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File stream status functions.
 */

#include "stdio/stdio.h"

/** Clear stream error and EOF status.
 * @param stream        Stream to clear. */
void clearerr(FILE *stream) {
    stream->eof = false;
    stream->err = false;
}

/** Get end-of-file status for file stream.
 * @param stream        Stream to get value from.
 * @return              End-of-file status. */
int feof(FILE *stream) {
    return stream->eof;
}

/** Get error status for file stream.
 * @param stream        Stream to get value from.
 * @return              Error status. */
int ferror(FILE *stream) {
    return stream->err;
}

/** Get a stream's file descriptor.
 * @param stream        Stream to get from.
 * @return              File descriptor that the stream is using. */
int fileno(FILE *stream) {
    return stream->fd;
}
