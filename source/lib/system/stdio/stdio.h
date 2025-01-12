/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Standard I/O private functions.
 */

#pragma once

#include <stdbool.h>
#include <stdio.h>

#include "libsystem.h"

/** Internal structure of an I/O stream (FILE). */
struct __fstream_internal {
    int fd;                         /**< File descriptor the stream refers to. */
    bool err;                       /**< Error indicator. */
    bool eof;                       /**< End of file indicator. */
    int pushback_ch;                /**< Character pushed back with ungetc(). */
    bool have_pushback;             /**< Set to true if there is a pushed back character. */
};

/** Arguments to do_scanf. */
struct scanf_args {
    int (*getch)(void *);           /**< Get a character from the source file/string. */
    int (*putch)(int, void *);      /**< Return a character to the source file/string. */
    void *data;                     /**< Data to pass to the helper functions. */
};

/** Type for a do_printf() helper function. */
typedef void (*printf_helper_t)(char, void *);

extern int do_vprintf(
    printf_helper_t helper, void *data, const char *restrict fmt,
    va_list args) __sys_hidden;
extern int do_printf(
    printf_helper_t helper, void *data, const char *restrict fmt,
    ...) __sys_hidden;

extern int do_scanf(struct scanf_args *data, const char *restrict fmt, va_list args) __sys_hidden;
