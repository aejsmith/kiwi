/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               Ext2 inode functions.
 */

#include <mm/malloc.h>

#include <status.h>
#include <time.h>

#include "ext2.h"

/** Read an inode from the filesystem.
 * @param mount         Mount to read from.
 * @param num           Inode number.
 * @param _inode        Where to store pointer to inode. */
status_t ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **_inode) {
    status_t ret;

    /* Get the group descriptor containing the inode. */
    uint32_t group = (num - 1) / mount->inodes_per_group;
    if (group >= mount->block_groups) {
        kprintf(LOG_WARN, "ext2: %pD: inode number %" PRIu32 " is invalid\n", mount->fs->device, num);
        return STATUS_CORRUPT_FS;
    }

    ext2_group_desc_t *group_desc = mount->group_table + (group * mount->group_desc_size);

    ext2_inode_t *inode = kmalloc(sizeof(*inode), MM_KERNEL);

    inode->mount = mount;
    inode->num   = num;

    /* Get the size of the inode and its offset in the group's inode table. */
    offset_t inode_block = le32_to_cpu(group_desc->bg_inode_table);
    if (mount->group_desc_size >= EXT2_MIN_GROUP_DESC_SIZE_64BIT)
        inode_block |= (offset_t)le32_to_cpu(group_desc->bg_inode_table_hi) << 32;
    inode->disk_offset =
        (inode_block * mount->block_size) +
        ((offset_t)((num - 1) % mount->inodes_per_group) * mount->inode_size);

    size_t bytes;
    ret = file_read(mount->fs->handle, &inode->disk, mount->inode_read_size, inode->disk_offset, &bytes);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "ext2: %pD: failed to read inode %" PRIu32 ": %d\n", mount->fs->device, num, ret);
        goto err_free;
    } else if (bytes != mount->inode_read_size) {
        kprintf(LOG_WARN, "ext2: %pD: incomplete read of inode %" PRIu32 "\n", mount->fs->device, num);
        ret = STATUS_CORRUPT_FS;
        goto err_free;
    }

    inode->size = le32_to_cpu(inode->disk.i_size_lo);
    if (le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFREG)
        inode->size |= (offset_t)le32_to_cpu(inode->disk.i_size_high) << 32;

    dprintf(
        "ext2: %pD: read inode %" PRIu32 " from %" PRIu64 " (group: %" PRIu32 ", block: %" PRId64 ")\n",
        mount->fs->device, num, inode->disk_offset, group, inode_block);

    *_inode = inode;
    return STATUS_SUCCESS;

err_free:
    kfree(inode);
    return ret;
}

/** Free an in-memory inode structure. */
void ext2_inode_put(ext2_inode_t *inode) {
    if (!(inode->mount->fs->flags & FS_MOUNT_READ_ONLY)) {
        if (le16_to_cpu(inode->disk.i_links_count) == 0) {
            kprintf(LOG_ERROR, "ext2_inode_put: TODO: free inode\n");
        }
    }

    kfree(inode);
}

static nstime_t decode_timestamp(ext2_inode_t *inode, uint32_t *_low, uint32_t *_high) {
    uint64_t seconds     = le32_to_cpu(*_low);
    uint64_t nanoseconds = 0;

    /* High part is valid if the on-disk inode size includes it. */
    size_t end = (uint8_t *)&_high[1] - (uint8_t *)&inode->disk;
    if (inode->mount->inode_size >= end) {
        uint32_t high = le32_to_cpu(*_high);
        seconds      |= (uint64_t)(high & 3) << 32;
        nanoseconds   = high >> 2;
    }

    return secs_to_nsecs(seconds) + nanoseconds;
}

/** Get the access time of an inode. */
nstime_t ext2_inode_atime(ext2_inode_t *inode) {
    return decode_timestamp(inode, &inode->disk.i_atime, &inode->disk.i_atime_extra);
}

/** Get the access time of an inode. */
nstime_t ext2_inode_ctime(ext2_inode_t *inode) {
    return decode_timestamp(inode, &inode->disk.i_ctime, &inode->disk.i_ctime_extra);
}

/** Get the modification time of an inode. */
nstime_t ext2_inode_mtime(ext2_inode_t *inode) {
    return decode_timestamp(inode, &inode->disk.i_mtime, &inode->disk.i_mtime_extra);
}
