/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File removal command.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: unlink file\n");
        return (argc != 2) ? EXIT_FAILURE : EXIT_SUCCESS;
    }

    int ret = unlink(argv[1]);
    if (ret != 0) {
        perror("unlink");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
