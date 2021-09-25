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
 * @brief               Filesystem mount utility.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void usage(void) {
    printf("Usage: mount [-d device] [-t type] [-r] path...\n");
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0) {
            usage();
            return EXIT_SUCCESS;
        }
    }

    uint32_t flags     = 0;
    const char *device = NULL;
    const char *type   = NULL;

    int c;
    while ((c = getopt(argc, argv, "d:t:r")) != -1) {
        switch (c) {
            case 'd':
                device = optarg;
                break;
            case 't':
                type = optarg;
                break;
            case 'r':
                flags |= FS_MOUNT_READ_ONLY;
                break;
            default:
                usage();
                return EXIT_FAILURE;
        }
    }

    /* Need one path argument. */
    if (optind != argc - 1) {
        usage();
        return EXIT_FAILURE;
    }

    /* At least one of device or type necessary. */
    if (!device && !type) {
        usage();
        return EXIT_FAILURE;
    }

    status_t ret = kern_fs_mount(device, argv[optind], type, flags, NULL);
    if (ret != STATUS_SUCCESS) {
        fprintf(stderr, "mount: failed to mount '%s': %s\n", argv[optind], kern_status_string(ret));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
