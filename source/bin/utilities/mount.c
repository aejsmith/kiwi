/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
