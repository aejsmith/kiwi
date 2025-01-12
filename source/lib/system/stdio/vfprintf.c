/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Formatted output functions.
 */

#include "stdio/stdio.h"

/** Helper for vfprintf(). */
static void vfprintf_helper(char ch, void *data) {
    fputc(ch, (FILE *)data);
}

/** Output a formatted string to a file stream.
 * @param stream        Stream to output to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int vfprintf(FILE *restrict stream, const char *restrict fmt, va_list args) {
    return do_vprintf(vfprintf_helper, stream, fmt, args);
}

/** Output a formatted string to a file stream.
 * @param stream        Stream to output to.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int fprintf(FILE *restrict stream, const char *restrict fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vfprintf(stream, fmt, args);
    va_end(args);

    return ret;
}

/** Output a formatted string to standard output.
 * @param fmt           Format string used to create the message.
 * @param args          Arguments to substitute into format.
 * @return              Number of characters printed. */
int vprintf(const char *restrict fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
}

/** Output a formatted string to standard output.
 * @param fmt           Format string used to create the message.
 * @param ...           Arguments to substitute into format.
 * @return              Number of characters printed. */
int printf(const char *restrict fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vprintf(fmt, args);
    va_end(args);

    return ret;
}
