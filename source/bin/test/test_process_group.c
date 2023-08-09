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
