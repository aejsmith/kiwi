/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Print error function.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>

/**
 * Print an error message.
 *
 * Prints the given error message followed by the string returned from
 * strerror() for the current errno value and a newline character to stderr. If
 * the message given is NULL, then only the string given by strerror() is
 * printed.
 *
 * @param s             Error message to print.
 */
void perror(const char *s) {
    if (s && s[0]) {
        fprintf(stderr, "%s: %s\n", s, strerror(errno));
    } else {
        fprintf(stderr, "%s\n", strerror(errno));
    }
}
