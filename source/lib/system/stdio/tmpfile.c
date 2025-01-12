/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Temporary file creation function.
 */

#include <stdio.h>

#include "libsystem.h"

FILE *tmpfile(void) {
    libsystem_stub("tmpfile", true);
    return NULL;
}

char *tmpnam(char *s) {
    libsystem_stub("tmpnam", true);
    return NULL;
}
