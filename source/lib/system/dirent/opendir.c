/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Open directory function.
 */

#include <stdlib.h>
#include <string.h>

#include "dirent/dirent.h"

/** Open a new directory stream.
 * @param path          Path to directory.
 * @return              Pointer to directory stream, or NULL on failure */
DIR *opendir(const char *path) {
    file_info_t info;
    status_t ret;
    DIR *dir;

    dir = malloc(sizeof(*dir));
    if (!dir)
        return NULL;

    ret = kern_fs_open(path, FILE_ACCESS_READ, 0, 0, &dir->handle);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        free(dir);
        return NULL;
    }

    ret = kern_file_info(dir->handle, &info);
    if (ret == STATUS_SUCCESS && info.type != FILE_TYPE_DIR)
        ret = STATUS_NOT_DIR;

    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        kern_handle_close(dir->handle);
        free(dir);
        return NULL;
    }

    return dir;
}
