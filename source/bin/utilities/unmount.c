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
 * @brief               Filesystem unmount utility.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    printf("Usage: unmount [-f] path...\n");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    uint32_t flags = 0;

    int c;
    while ((c = getopt(argc, argv, "f")) != -1) {
        switch (c) {
            case 'f':
                flags |= FS_UNMOUNT_FORCE;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need at least one path argument. */
    if (optind >= argc) {
        usage();
        return EXIT_FAILURE;
    }

    int status = EXIT_SUCCESS;

    for (int i = optind; i < argc; i++) {
        status_t ret = kern_fs_unmount(argv[i], flags);
        if (ret != STATUS_SUCCESS) {
            fprintf(stderr, "unmount: failed to unmount '%s': %s\n", argv[i], kern_status_string(ret));
            status = EXIT_FAILURE;
        }
    }

    return status;
}
