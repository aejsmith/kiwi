/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File seek functions.
 */

#include <unistd.h>

#include "stdio/stdio.h"

/** Reposition a stream's file pointer.
 * @param stream        Stream to reposition.
 * @param off           New offset.
 * @param act           How to set the offset.
 * @return              0 on success, -1 on failure. */
int fseeko(FILE *stream, off_t off, int act) {
    off_t ret;

    ret = lseek(stream->fd, off, act);
    if (ret < 0)
        return -1;

    return 0;
}

/** Reposition a stream's file pointer.
 * @param stream        Stream to reposition.
 * @param off           New offset.
 * @param act           How to set the offset.
 * @return              0 on success, -1 on failure. */
int fseek(FILE *stream, long off, int act) {
    return fseeko(stream, (off_t)off, act);
}

/** Set file pointer to beginning of file
 * @param stream        Stream to reposition. */
void rewind(FILE *stream) {
    fseek(stream, 0, SEEK_SET);
    clearerr(stream);
}

/** Get a stream's file pointer.
 * @param stream        Stream to get pointer for.
 * @return              File pointer on success, -1 on failure. */
off_t ftello(FILE *stream) {
    return lseek(stream->fd, 0, SEEK_CUR);
}

/** Get a stream's file pointer.
 * @param stream        Stream to get pointer for.
 * @return              File pointer on success, -1 on failure. */
long ftell(FILE *stream) {
    return (long)lseek(stream->fd, 0, SEEK_CUR);
}
