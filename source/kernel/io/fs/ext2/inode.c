/*
 * Copyright (C) 2009-2023 Alex Smith
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

#include <lib/string.h>

#include <mm/malloc.h>

#include <status.h>
#include <time.h>

#include "ext2.h"

static status_t find_leaf_extent(
    ext2_inode_t *inode, ext4_extent_header_t *header, uint32_t num, void *buf,
    ext4_extent_header_t **_header)
{
    while (true) {
        if (le16_to_cpu(header->eh_magic) != EXT4_EXT_MAGIC) {
            return STATUS_CORRUPT_FS;
        } else if (!le16_to_cpu(header->eh_depth)) {
            *_header = header;
            return STATUS_SUCCESS;
        }

        ext4_extent_idx_t *index = (ext4_extent_idx_t *)&header[1];
        uint16_t i;

        for (i = 0; i < le16_to_cpu(header->eh_entries); i++) {
            if (num < le32_to_cpu(index[i].ei_block))
                break;
        }

        if (i == 0)
            return STATUS_CORRUPT_FS;

        status_t ret = ext2_block_read(inode->mount, buf, le32_to_cpu(index[i - 1].ei_leaf));
        if (ret != STATUS_SUCCESS)
            return ret;

        header = (ext4_extent_header_t *)buf;
    }
}

static status_t lookup_block_extent(ext2_inode_t *inode, uint64_t num, uint64_t *_raw) {
    void *buf __cleanup_kfree = kmalloc(inode->mount->block_size, MM_KERNEL);

    ext4_extent_header_t *header = (ext4_extent_header_t *)inode->disk.i_block;

    status_t ret = find_leaf_extent(inode, header, num, buf, &header);
    if (ret != STATUS_SUCCESS)
        return ret;

    ext4_extent_t *extent = (ext4_extent_t *)&header[1];
    uint16_t i;

    for (i = 0; i < le16_to_cpu(header->eh_entries); i++) {
        if (num < le32_to_cpu(extent[i].ee_block))
            break;
    }

    if (i == 0)
        return STATUS_CORRUPT_FS;

    num -= le32_to_cpu(extent[i - 1].ee_block);

    *_raw = (num < le16_to_cpu(extent[i - 1].ee_len))
        ? num + le32_to_cpu(extent[i - 1].ee_start)
        : 0;

    return STATUS_SUCCESS;
}

static status_t lookup_block_map(ext2_inode_t *inode, uint64_t num, uint64_t *_raw) {
    status_t ret;

    /* First check if it's a direct block in the inode structure. */
    if (num < EXT2_NDIR_BLOCKS) {
        *_raw = le32_to_cpu(inode->disk.i_block[num]);
        return STATUS_SUCCESS;
    }

    uint32_t *buf __cleanup_kfree = kmalloc(inode->mount->block_size, MM_KERNEL);

    num -= EXT2_NDIR_BLOCKS;

    /* Check whether the indirect block contains the block. The indirect block
     * contains as many 32-bit entries as will fit in one FS block. */
    uint32_t inodes_per_block = inode->mount->block_size / sizeof(uint32_t);
    if (num < inodes_per_block) {
        uint32_t ind = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
        if (ind == 0) {
            *_raw = 0;
            return STATUS_SUCCESS;
        }

        ret = ext2_block_read(inode->mount, buf, ind);
        if (ret != STATUS_SUCCESS)
            return ret;

        *_raw = le32_to_cpu(buf[num]);
        return STATUS_SUCCESS;
    }

    num -= inodes_per_block;

    /* Check the double-indirect blocks. The double-indirect block contains as
     * many 32-bit entries as will fit in one FS block, with each entry pointing
     * to an indirect block. */
    if (num < (inodes_per_block * inodes_per_block)) {
        uint32_t dind = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK]);
        if (dind == 0) {
            *_raw = 0;
            return STATUS_SUCCESS;
        }

        ret = ext2_block_read(inode->mount, buf, dind);
        if (ret != STATUS_SUCCESS)
            return ret;

        /* Get indirect block inside double-indirect block. */
        uint32_t ind = le32_to_cpu(buf[num / inodes_per_block]);
        if (ind == 0) {
            *_raw = 0;
            return STATUS_SUCCESS;
        }

        ret = ext2_block_read(inode->mount, buf, ind);
        if (ret != STATUS_SUCCESS)
            return ret;

        *_raw = le32_to_cpu(buf[num % inodes_per_block]);
        return STATUS_SUCCESS;
    }

    // TODO: Triple indirect block.
    kprintf(LOG_ERROR, "ext2: %pD: tri-indirect blocks not yet supported!\n", inode->mount->fs->device);
    return STATUS_NOT_SUPPORTED;
}

static status_t ext2_file_map_lookup(file_map_t *map, uint64_t num, uint64_t *_raw) {
    ext2_inode_t *inode = map->private;

    if (le32_to_cpu(inode->disk.i_flags) & EXT4_EXTENTS_FL) {
        return lookup_block_extent(inode, num, _raw);
    } else {
        return lookup_block_map(inode, num, _raw);
    }
}

static status_t ext2_file_map_read_block(file_map_t *map, void *buf, uint64_t num) {
    ext2_inode_t *inode = map->private;

    if (num == 0) {
        /* Sparse block, fill with zeros. */
        memset(buf, 0, inode->mount->block_size);
        return STATUS_SUCCESS;
    } else {
        return ext2_block_read(inode->mount, buf, num);
    }
}

static status_t ext2_file_map_write_block(file_map_t *map, const void *buf, uint64_t num) {
    return STATUS_NOT_IMPLEMENTED;
}

static const file_map_ops_t ext2_file_map_ops = {
    .lookup      = ext2_file_map_lookup,
    .read_block  = ext2_file_map_read_block,
    .write_block = ext2_file_map_write_block,
};

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

    inode->map   = file_map_create(mount->block_size, &ext2_file_map_ops, inode);
    inode->cache = page_cache_create(inode->size, &file_map_page_cache_ops, inode->map);

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
    bool is_free = false;

    if (!(inode->mount->fs->flags & FS_MOUNT_READ_ONLY)) {
        is_free = le16_to_cpu(inode->disk.i_links_count) == 0;
        if (is_free) {
            kprintf(LOG_ERROR, "ext2_inode_put: TODO: free inode\n");
        }

        // TODO: Write back inode. Need dirty tracking?
    }

    /* Discard outstanding writes if the file is free. */
    // TODO: How to handle write errors? When we have a parent disk cache, it
    // should be flushed to there and we won't lose data.
    // TODO: When do we free the blocks if removed? Must be here.
    status_t ret = page_cache_destroy(inode->cache);
    if (ret != STATUS_SUCCESS) {
        kprintf(
            LOG_ERROR, "ext2: %pD: failed to write cache for inode %" PRIu32 "\n",
            inode->mount->fs->device, inode->num);
    }

    file_map_destroy(inode->map);

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
