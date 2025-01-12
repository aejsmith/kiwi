/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               File concatenation command.
 */

#include <sys/stat.h>

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool cat_file(const char *file) {
    int fd;
    if (strcmp(file, "-") == 0) {
        fd = STDIN_FILENO;
    } else {
        fd = open(file, O_RDONLY);
        if (fd < 0) {
            perror("cat: open");
            return false;
        }
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("cat: fstat");
        close(fd);
        return false;
    }

    if (st.st_blksize == 0) {
        fprintf(stderr, "cat: warning: st_blksize is 0\n");
        st.st_blksize = 1;
    }

    char *buf = malloc(st.st_blksize);
    if (!buf) {
        perror("cat: malloc");
        close(fd);
        return false;
    }

    bool success = true;
    while (true) {
        ssize_t ret = read(fd, buf, st.st_blksize);
        if (ret < 0) {
            perror("cat: read");
            success = false;
            break;
        } else if (ret == 0) {
            break;
        }

        fwrite(buf, ret, 1, stdout);
    }

    free(buf);
    close(fd);
    return success;
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--help") == 0) {
        printf("Usage: cat file...\n");
        return EXIT_SUCCESS;
    }

    int ret = EXIT_SUCCESS;

    if (argc < 2) {
        if (!cat_file("-"))
            ret = EXIT_FAILURE;
    } else {
        for (int i = 1; i < argc; i++) {
            if (!cat_file(argv[i]))
                ret = EXIT_FAILURE;
        }
    }

    return ret;
}
