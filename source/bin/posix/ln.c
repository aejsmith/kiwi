/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               Link creation command.
 */

#include <core/path.h>

#include <sys/stat.h>

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool use_symlink = false;
static bool force_creation = false;

static void usage(void) {
    printf("Usage: ln [-fs] source_file target_file\n");
    printf("       ln [-fs] source_file... target_dir\n");
}

static bool do_link(char *target, char *name) {
    if (access(name, F_OK) == 0) {
        if (force_creation) {
            if (unlink(name) != 0) {
                fprintf(stderr, "ln: removing existing file %s: %s\n", name, strerror(errno));
                return false;
            }
        } else {
            fprintf(
                stderr, "ln: creating %slink %s: %s\n",
                (use_symlink) ? "symbolic " : "", name, strerror(EEXIST));
            return false;
        }
    }

    if (use_symlink) {
        if (symlink(target, name) != 0) {
            fprintf(stderr, "ln: creating symbolic link %s: %s\n", name, strerror(errno));
            return false;
        }
    } else {
        /* FIXME: If source_file is a symbolic link, actions shall be performed
         * equivalent to the link() function using the object that source_file
         * references as the path1 argument and the destination path as the
         * path2 argument. */
        if (link(target, name) != 0) {
            fprintf(stderr, "ln: creating link %s: %s\n", name, strerror(errno));
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    int c;
    while ((c = getopt(argc, argv, "fs")) != -1) {
        switch (c) {
            case 'f':
                force_creation = true;
                break;
            case 's':
                use_symlink = true;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need at least two file arguments. */
    if ((argc - optind) < 2) {
        usage();
        return EXIT_FAILURE;
    }

    /* Check if the destination is a directory. */
    bool is_dir = false;
    struct stat st;
    if (stat(argv[argc - 1], &st) != 0 && errno != ENOENT) {
        perror("ln: stat");
        return EXIT_FAILURE;
    } else if (errno != ENOENT && S_ISDIR(st.st_mode)) {
        is_dir = true;
    }

    if (!is_dir && (argc - optind) != 2) {
        usage();
        return EXIT_FAILURE;
    }

    int ret = EXIT_SUCCESS;

    for (int i = optind; i < (argc - 1); i++) {
        /* Work out the destination path. */
        if (is_dir) {
            char *name = core_path_basename(argv[i]);
            if (!name) {
                errno = ENOMEM;
                perror("ln: malloc");
                continue;
            }

            char *dest = malloc(strlen(name) + strlen(argv[argc - 1]) + 2);
            if (!dest) {
                free(name);
                perror("ln: malloc");
                continue;
            }

            sprintf(dest, "%s/%s", argv[argc - 1], name);

            if (!do_link(argv[i], dest))
                ret = EXIT_FAILURE;

            free(dest);
            free(name);
        } else {
            if (!do_link(argv[i], argv[argc - 1]))
                ret = EXIT_FAILURE;
        }
    }

    return ret;
}
