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
 * @brief               Directory creation command.
 */

#include <core/path.h>

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/** Mode to create with. */
static mode_t mkdir_mode = 0777;

/** Whether to create missing directories. */
static bool create_missing = false;

static void usage(void) {
    printf("Usage: mkdir [-p] [-m mode] dir...\n");
}

static bool do_mkdir(const char *path) {
    if (mkdir(path, mkdir_mode) == 0)
        return true;

    if (errno == ENOENT && create_missing) {
        char *dir = core_path_dirname(path);
        if (!dir) {
            errno = ENOMEM;
            perror("mkdir: malloc");
            return false;
        }

        bool success = do_mkdir(dir);
        free(dir);
        if (!success)
            return false;

        if (mkdir(path, mkdir_mode) != 0) {
            fprintf(stderr, "mkdir: cannot create directory '%s': %s\n", path, strerror(errno));
            return false;
        }

        return true;
    } else if (errno == EEXIST && create_missing) {
        return true;
    } else {
        fprintf(stderr, "mkdir: cannot create directory '%s': %s\n", path, strerror(errno));
        return false;
    }
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    int c;
    while ((c = getopt(argc, argv, "pm:")) != -1) {
        switch (c) {
            case 'p':
                create_missing = true;
                break;
            case 'm':
                if (optarg[0] != '0') {
                    fprintf(stderr, "mkdir: TODO: non-octal mode strings are not yet supported: %s\n", optarg);
                    return EXIT_FAILURE;
                }

                mkdir_mode = strtol(optarg, NULL, 8);
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need at least one name argument. */
    if ((argc - optind) < 1) {
        usage();
        return EXIT_FAILURE;
    }

    /* Go through everything specified. */
    int ret = EXIT_SUCCESS;
    for (int i = optind; i < argc; i++) {
        if (!do_mkdir(argv[i]))
            ret = EXIT_FAILURE;
    }

    return ret;
}
