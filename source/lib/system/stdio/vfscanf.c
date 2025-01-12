/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               String unformatting function.
 */

#include <stdarg.h>
#include <stdio.h>

#include "stdio/stdio.h"

/**
 * Unformat data from a stream.
 *
 * Unformats data from a file stream into a list of arguments according to the
 * given format string.
 *
 * @param stream        Stream to read from.
 * @param fmt           Format string.
 * @param args          Pointers to values to set to unformatted arguments.
 *
 * @return              Number of input items matched.
 */
int vfscanf(FILE *restrict stream, const char *restrict fmt, va_list args) {
    struct scanf_args sdata = {
        (int (*)(void *))fgetc,
        (int (*)(int, void *))ungetc,
        stream
    };

    return do_scanf(&sdata, fmt, args);
}

/**
 * Unformat data from a stream.
 *
 * Unformats data from a file stream into a list of arguments according to the
 * given format string.
 *
 * @param stream        Stream to read from.
 * @param fmt           Format string.
 * @param ...           Pointers to values to set to unformatted arguments.
 *
 * @return              Number of input items matched.
 */
int fscanf(FILE *restrict stream, const char *restrict fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vfscanf(stream, fmt, args);
    va_end(args);

    return ret;
}

/**
 * Unformat data from standard input.
 *
 * Unformats data from standard input into a list of arguments according to
 * the given format string.
 *
 * @param fmt           Format string.
 * @param args          Pointers to values to set to unformatted arguments.
 *
 * @return              Number of input items matched.
 */
int vscanf(const char *restrict fmt, va_list args) {
    return vfscanf(stdin, fmt, args);
}

/**
 * Unformat data from standard input.
 *
 * Unformats data from standard input into a list of arguments according to
 * the given format string.
 *
 * @param fmt           Format string.
 * @param ...           Pointers to values to set to unformatted arguments.
 *
 * @return              Number of input items matched.
 */
int scanf(const char *restrict fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vfscanf(stdin, fmt, args);
    va_end(args);

    return ret;
}
