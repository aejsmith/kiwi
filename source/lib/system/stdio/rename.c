/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Rename file function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdio.h>

#include "libsystem.h"

/** Rename a filesystem entry.
 * @param source        Path to rename.
 * @param dest          Path to rename to.
 * @return              0 on success, -1 on failure. */
int rename(const char *source, const char *dest) {
    status_t ret;

    ret = kern_fs_rename(source, dest);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    return 0;
}
