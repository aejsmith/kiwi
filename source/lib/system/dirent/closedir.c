/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Close directory function.
 */

#include <stdlib.h>
#include "dirent/dirent.h"

/** Close a directory stream.
 * @param dir           Directory stream to close.
 * @return              0 on success, -1 on failure. */
int closedir(DIR *dir) {
    status_t ret;

    ret = kern_handle_close(dir->handle);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        return -1;
    }

    free(dir);
    return 0;
}
