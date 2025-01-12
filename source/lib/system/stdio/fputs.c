/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Put string functions.
 */

#include <string.h>

#include "stdio/stdio.h"

/** Write a string to a file stream.
 * @param s             String to write.
 * @param stream        Stream to write to.
 * @return              0 on success, EOF on failure or EOF. */
int fputs(const char *restrict s, FILE *restrict stream) {
    if (fwrite(s, strlen(s), 1, stream) != 1)
        return EOF;

    return 0;
}

/** Write a string to standard output.
 * @param s             String to write.
 * @return              0 on success, EOF on failure or EOF. */
int puts(const char *s) {
    if (fputs(s, stdout) != 0)
        return EOF;

    if (fputc('\n', stdout) != '\n')
        return EOF;

    return 0;
}
