/*
 * Copyright (C) 2009-2021 Alex Smith
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
