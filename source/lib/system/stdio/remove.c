/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Path remove function.
 */

#include <stdio.h>
#include <unistd.h>

/** Remove a path from the filesystem.
 * @param path          Path to remove.
 * @return              0 on success, -1 on failure. */
int remove(const char *path) {
    /* Our unlink implementation supports directory removal. */
    return unlink(path);
}
