/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Directory removal command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: rmdir dir...\n");
        return (argc < 2) ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    for (int i = 1; i < argc; i++) {
        int ret = rmdir(argv[i]);
        if (ret != 0) {
            perror("rmdir");
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
