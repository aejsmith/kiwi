/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Ext2 directory functions.
 *
 * TODO:
 *  - Support Ext3 hash tree directories.
 *
 * Write support notes:
 *  - read_dir implementation assumes that byte offsets will always point to a
 *    valid dir_entry. OK if when removing entries we don't coalesce entries,
 *    but if we do that we'll run into problems. Linux appears to resolve this
 *    with a version number, and rescans from the start of a directory block
 *    for a valid entry offset if the version has changed.
 */

#include <mm/malloc.h>

#include <status.h>

#include "ext2.h"

static status_t read_dir_entry(ext2_inode_t *inode, ext2_dir_entry_t *entry, offset_t offset, char *name) {
    status_t ret;

    size_t bytes;
    ret = ext2_inode_read(inode, entry, sizeof(*entry), offset, &bytes);
    if (ret != STATUS_SUCCESS) {
        return ret;
    } else if (bytes != sizeof(*entry)) {
        return STATUS_CORRUPT_FS;
    }

    if (le16_to_cpu(entry->rec_len) < sizeof(ext2_dir_entry_t))
        return STATUS_CORRUPT_FS;

    if (name) {
        offset += sizeof(ext2_dir_entry_t);

        ret = ext2_inode_read(inode, name, entry->name_len, offset, &bytes);
        if (ret != STATUS_SUCCESS) {
            return ret;
        } else if (bytes != entry->name_len) {
            return STATUS_CORRUPT_FS;
        }

        name[entry->name_len] = 0;
    }

    return STATUS_SUCCESS;
}

/** Iterate through entries in an Ext2 directory.
 * @param inode         Node to iterate through.
 * @param offset        Byte offset of the directory entry to start at.
 * @param cb            Callback function.
 * @param arg           Argument to pass to callback.
 * @return              Status code describing result of the operation. */
status_t ext2_dir_iterate(ext2_inode_t *inode, offset_t offset, ext2_dir_iterate_cb_t cb, void *arg) {
    status_t ret;

    char *name __cleanup_kfree = kmalloc(EXT2_NAME_MAX + 1, MM_KERNEL);

    while (offset < inode->size) {
        ext2_dir_entry_t entry;
        ret = read_dir_entry(inode, &entry, offset, name);
        if (ret != STATUS_SUCCESS)
            return ret;

        if (entry.inode != 0 && entry.file_type != EXT2_FT_UNKNOWN && entry.name_len != 0) {
            if (!cb(inode, &entry, name, offset, arg))
                return STATUS_SUCCESS;
        }

        offset += le16_to_cpu(entry.rec_len);
    }

    return STATUS_SUCCESS;
}
