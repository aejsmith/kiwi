/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Option parsing function.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

char *optarg;
int optind = 1, opterr = 1, optopt;

/**
 * Parses command line options according to the provided option string. The
 * option string should be a string of valid option characters. If an option
 * requires an argument, the character should be followed by a : * character in
 * the string.
 *
 * @param argc          Argument count.
 * @param argv          Argument array.
 * @param opts          Argument string.
 *
 * @return              Option character found, '?' if unknown character, ':'
 *                      if missing an argument and the first character of opts
 *                      was a colon ('?' if missing and first character was not
 *                      a colon), and -1 when option parsing is finished.
 */
int getopt(int argc, char *const argv[], const char *opts) {
    static int offset = 1;

    if (optind >= argc || !argv[optind] || *argv[optind] != '-' || strcmp(argv[optind], "-") == 0) {
        return -1;
    } else if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    char *tmp = argv[optind] + offset++;
    int ret   = *tmp++;
    char *ptr = strchr(opts, ret);
    if (!ptr) {
        optopt = ret;
        if (opterr != 0)
            fprintf(stderr, "%s: illegal option -- %c\n", argv[0], ret);
        ret = '?';
        goto out;
    }

    if (ptr[1] == ':') {
        if (*tmp) {
            optarg = tmp;
            optind++;
            offset = 1;
            return ret;
        } else {
            if (optind + 1 >= argc) {
                fprintf(stderr, "%s: option requires an argument -- %c\n", argv[0], ret);
                ret = (*opts == ':') ? ':' : '?';
            } else {
                optarg = argv[++optind];
                optind++;
                offset = 1;
                return ret;
            }
        }
    }

out:
    if (!*(argv[optind] + offset)) {
        offset = 1;
        optind++;
    }

    return ret;
}
