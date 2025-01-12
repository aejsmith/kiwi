/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               POSIX stub functions.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libsystem.h"

FILE *popen(const char *command, const char *mode) {
    libsystem_stub(__func__, false);
    return NULL;
}

int pclose(FILE *stream) {
    libsystem_stub(__func__, false);
    return -1;
}

int gethostname(char *name, size_t len) {
    strncpy(name, "kiwi", len);
    return 0;
}
