/*
 * Copyright (C) 2009-2022 Alex Smith
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
