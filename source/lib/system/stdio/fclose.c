/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File close function.
 */

#include <stdlib.h>
#include <unistd.h>

#include "stdio/stdio.h"

/** Close a file stream.
 * @param stream        File stream to close.
 * @return              0 on success, EOF on failure. */
int fclose(FILE *stream) {
    if (close(stream->fd) != 0)
        return EOF;

    free(stream);
    return 0;
}
