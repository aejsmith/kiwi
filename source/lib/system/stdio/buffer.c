/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File buffer functions.
 */

#include <errno.h>
#include "stdio/stdio.h"

/** Set a stream's buffering mode.
 * @param stream        Stream to set mode for.
 * @param buf           If not NULL, the stream will be set to fully buffered
 *                      mode with buf as the buffer. If NULL, buffering will
 *                      be disabled. */
void setbuf(FILE *restrict stream, char *restrict buf) {
    setvbuf(stream, buf, (buf) ? _IOFBF : _IONBF, BUFSIZ);
}

/** Set a stream's buffering mode.
 * @param stream        Stream to set mode for.
 * @param buf           If not NULL, a preallocated buffer to use.
 * @param mode          Buffering mode.
 * @param size          Size of the provided buffer.
 * @return              0 on success, -1 on failure with errno set. */
int setvbuf(FILE *restrict stream, char *restrict buf, int mode, size_t size) {
    switch (mode) {
    case _IONBF:
        break;
    case _IOLBF:
    case _IOFBF:
        //libsystem_stub("setvbuf", true);
        return -1;
    default:
        errno = EINVAL;
        return -1;
    }

    return 0;
}
