/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Read directory function.
 */

#include <stdlib.h>
#include <string.h>

#include "dirent/dirent.h"

/** Read a directory entry.
 * @param dir           Directory stream to read from.
 * @return              Pointer to directory info structure, or NULL on failure.
 *                      Data returned may be overwritten by a subsequent call
 *                      to readdir(). */
struct dirent *readdir(DIR *dir) {
    dir_entry_t *entry;
    struct dirent *dent;
    status_t ret;

    entry = malloc(DIRSTREAM_BUF_SIZE);
    if (!entry)
        return NULL;

    ret = kern_file_read_dir(dir->handle, entry, DIRSTREAM_BUF_SIZE);
    if (ret != STATUS_SUCCESS) {
        if (ret != STATUS_NOT_FOUND)
            libsystem_status_to_errno(ret);

        return NULL;
    }

    /* Convert the kernel entry structure to a dirent structure. */
    dent = (struct dirent *)dir->buf;
    dent->d_ino = entry->id;
    dent->d_reclen = sizeof(*dent) + strlen(entry->name) + 1;
    strcpy(dent->d_name, entry->name);
    free(entry);
    return dent;
}
