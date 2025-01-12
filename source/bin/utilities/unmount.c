/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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
