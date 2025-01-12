/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Filesystem functions.
 */

#include <kernel/fs.h>

#include "libkernel.h"

/** Get the current working directory path.
 * @param buf           Buffer to write path string to.
 * @param size          Size of buffer. If this is too small, STATUS_TOO_SMALL
 *                      will be returned.
 * @return              Status code describing result of the operation. */
__sys_export status_t kern_fs_curr_dir(char *buf, size_t size) {
    return kern_fs_path(INVALID_HANDLE, buf, size);
}
