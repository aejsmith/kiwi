/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX process groups test.
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv) {
    pid_t pid = getpid();
    printf("getpid() returned %d\n", pid);

    pid_t pgid = getpgrp();
    printf("getpgrp() returned %d%s%s\n", pgid, (pgid < 0) ? ": " : "", (pgid < 0) ? strerror(errno) : "");

    int ret = setpgrp();
    if (ret != 0) {
        perror("setpgrp");
        return EXIT_FAILURE;
    }

    printf("setpgrp() succeeded\n");

    pgid = getpgrp();
    if (ret != 0) {
        perror("getpgrp");
        return EXIT_FAILURE;
    }

    printf("getpgrp() returned %d\n", pgid);

    pid_t child = fork();
    if (child < 0) {
        perror("fork");
        return EXIT_FAILURE;
    } else if (child == 0) {
        printf("fork() succeeded\n");

        pgid = getpgrp();
        if (ret != 0) {
            perror("getpgrp");
            return EXIT_FAILURE;
        }

        printf("getpgrp() in child returned %d\n", pgid);

        ret = setpgrp();
        if (ret != 0) {
            perror("setpgrp");
            return EXIT_FAILURE;
        }

        printf("setpgrp() in child succeeded\n");

        pgid = getpgrp();
        if (ret != 0) {
            perror("getpgrp");
            return EXIT_FAILURE;
        }

        printf("getpgrp() in child returned %d\n", pgid);

        return EXIT_SUCCESS;
    }

    wait(NULL);

    return EXIT_SUCCESS;
}
