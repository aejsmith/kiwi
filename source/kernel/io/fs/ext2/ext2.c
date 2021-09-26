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
 * @brief               Ext2 filesystem support.
 */

#include <device/device.h>

#include <io/fs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <module.h>
#include <status.h>

#include "ext2.h"

static void ext2_unmount(fs_mount_t *_mount) {
    ext2_mount_t *mount = _mount->private;

    //if (!(mount->fs->flags & FS_MOUNT_READ_ONLY)) {
    //    mount->sb.s_state = cpu_to_le16(EXT2_VALID_FS);
    //    ext2_mount_flush(mount);
    //}
    kprintf(LOG_DEBUG, "ext2_unmount: TODO\n");

    kfree(mount);
}

static status_t ext2_flush(fs_mount_t *_mount) {
    kprintf(LOG_DEBUG, "ext2_flush: TODO\n");
    return STATUS_NOT_IMPLEMENTED;
}

static status_t ext2_read_node(fs_mount_t *_mount, fs_node_t *node) {
    //kprintf(LOG_DEBUG, "ext2_read_node: TODO\n");
    //return STATUS_NOT_IMPLEMENTED;
    return STATUS_SUCCESS;
}

static fs_mount_ops_t ext2_mount_ops = {
    .unmount   = ext2_unmount,
    .flush     = ext2_flush,
    .read_node = ext2_read_node,
};

static bool ext2_probe(device_t *device, object_handle_t *handle, const char *uuid) {
    ext2_superblock_t *sb __cleanup_kfree = kmalloc(sizeof(ext2_superblock_t), MM_KERNEL);

    size_t bytes;
    if (file_read(handle, sb, sizeof(*sb), EXT2_SUPERBLOCK_OFFSET, &bytes) != STATUS_SUCCESS) {
        return false;
    } else if (bytes != sizeof(*sb) || le16_to_cpu(sb->s_magic) != EXT2_MAGIC) {
        return false;
    }

    /* Check if the revision is supported. We require DYNAMIC_REV for UUID
     * support. */
    uint32_t revision = le32_to_cpu(sb->s_rev_level);
    if (revision != EXT2_DYNAMIC_REV) {
        kprintf(LOG_NOTICE, "ext2: device %pD has unsupported revision %" PRIu32 "\n", device, revision);
        return false;
    }

    /* Check for incompatible features. */
    uint32_t feature_incompat = le32_to_cpu(sb->s_feature_incompat);
    if (feature_incompat & ~(uint32_t)EXT2_FEATURE_INCOMPAT_SUPP) {
        kprintf(
            LOG_NOTICE, "ext2: device %pD has unsupported incompatible features 0x%x\n",
            device, feature_incompat);

        return false;
    }

    /* Check the UUID if required. */
    if (uuid) {
        char str[UUID_STR_LEN + 1];
        snprintf(str, sizeof(str), "%pU", sb->s_uuid);
        if (strcmp(str, uuid) != 0)
            return false;
    }

    return true;
}

static status_t ext2_mount(fs_mount_t *_mount, fs_mount_option_t *opts, size_t count) {
    status_t ret;
    size_t bytes;

    ext2_mount_t *mount = kmalloc(sizeof(*mount), MM_KERNEL | MM_ZERO);

    mount->fs          = _mount;
    mount->fs->private = mount;
    mount->fs->ops     = &ext2_mount_ops;

    ret = file_read(mount->fs->handle, &mount->sb, sizeof(mount->sb), EXT2_SUPERBLOCK_OFFSET, &bytes);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "ext2: failed to read superblock for device %pD: %d\n", mount->fs->device, ret);
        goto err_free;
    } else if (bytes != sizeof(mount->sb)) {
        ret = STATUS_CORRUPT_FS;
        goto err_free;
    }

    /* Filesystem has already been verified as ext2 by ext2_probe(). */
    uint32_t feature_incompat  = le32_to_cpu(mount->sb.s_feature_incompat);
    uint32_t feature_ro_compat = le32_to_cpu(mount->sb.s_feature_ro_compat);

    /* If not mounting read-only, check for read-only features, and whether
     * the FS is clean. */
    if (!(mount->fs->flags & FS_MOUNT_READ_ONLY)) {
        if (feature_ro_compat & ~(uint32_t)EXT2_FEATURE_RO_COMPAT_SUPP) {
            kprintf(
                LOG_WARN, "ext2: device %pD has unsupported write features 0x%x, mounting read-only\n",
                mount->fs->device, feature_ro_compat);

            mount->fs->flags |= FS_MOUNT_READ_ONLY;
        } else if (le16_to_cpu(mount->sb.s_state) != EXT2_VALID_FS) {
            kprintf(
                LOG_WARN, "ext2: device %pD was not cleanly unmounted or is damaged, mounting read-only\n",
                mount->fs->device);

            mount->fs->flags |= FS_MOUNT_READ_ONLY;
        }
    }

    // TODO: enable write support
    mount->fs->flags |= FS_MOUNT_READ_ONLY;

    mount->inodes_per_group = le32_to_cpu(mount->sb.s_inodes_per_group);
    mount->inode_count      = le32_to_cpu(mount->sb.s_inodes_count);
    mount->blocks_per_group = le32_to_cpu(mount->sb.s_blocks_per_group);
    mount->block_count      = le32_to_cpu(mount->sb.s_blocks_count);
    mount->block_size       = 1024 << le32_to_cpu(mount->sb.s_log_block_size);
    mount->block_groups     = mount->inode_count / mount->inodes_per_group;
    mount->inode_size       = le16_to_cpu(mount->sb.s_inode_size);

    if (mount->block_size > PAGE_SIZE) {
        kprintf(
            LOG_WARN, "ext2: device %pD has unsupported block size %" PRIu32 " greater than system page size\n",
            mount->fs->device, mount->block_size);

        ret = STATUS_NOT_SUPPORTED;
        goto err_free;
    }

    dprintf("ext2: mounting filesystem from device %pD\n", mount->fs->device);
    dprintf(" block_size:   %u\n", mount->block_size);
    dprintf(" block_groups: %u\n", mount->block_groups);
    dprintf(" block_count:  %u\n", mount->block_count);
    dprintf(" inode_size:   %u\n", mount->inode_size);
    dprintf(" inode_count:  %u\n", mount->inode_count);

    if (feature_incompat & EXT4_FEATURE_INCOMPAT_64BIT) {
        mount->group_desc_size = le16_to_cpu(mount->sb.s_desc_size);

        if (mount->group_desc_size < EXT2_MIN_GROUP_DESC_SIZE_64BIT ||
            mount->group_desc_size > EXT2_MAX_GROUP_DESC_SIZE ||
            !is_pow2(mount->group_desc_size))
        {
            kprintf(
                LOG_WARN, "ext2: device %pD has unsupported group descriptor size %" PRIu32 "\n",
                mount->fs->device, mount->group_desc_size);

            ret = STATUS_CORRUPT_FS;
            goto err_free;
        }
    } else {
        mount->group_desc_size = EXT2_MIN_GROUP_DESC_SIZE;
    }

    mount->group_table_offset = mount->block_size * (le32_to_cpu(mount->sb.s_first_data_block) + 1);
    mount->group_table_size   = round_up(mount->block_groups * mount->group_desc_size, mount->block_size);

    /* Read in the group descriptor table. This could be very large. */
    mount->group_table = kmalloc(mount->group_table_size, MM_KERNEL_NOWAIT);
    if (!mount->group_table) {
        ret = STATUS_NO_MEMORY;
        goto err_free;
    }

    ret = file_read(mount->fs->handle, mount->group_table, mount->group_table_size, mount->group_table_offset, &bytes);
    if (ret != STATUS_SUCCESS) {
        kprintf(LOG_WARN, "ext2: failed to read group table for device %pD: %d\n", mount->fs->device, ret);
        goto err_free;
    } else if (bytes != mount->group_table_size) {
        kprintf(LOG_WARN, "ext2: incomplete read of group table for device %pD\n", mount->fs->device);
        ret = STATUS_CORRUPT_FS;
        goto err_free;
    }

    /* If mounting read-write, write back the superblock as mounted. */
    if (!(mount->fs->flags & FS_MOUNT_READ_ONLY)) {
        mount->sb.s_state     = cpu_to_le16(EXT2_ERROR_FS);
        mount->sb.s_mnt_count = cpu_to_le16(le16_to_cpu(mount->sb.s_mnt_count) + 1);

        ret = file_write(mount->fs->handle, &mount->sb, sizeof(mount->sb), EXT2_SUPERBLOCK_OFFSET, &bytes);
        if (ret != STATUS_SUCCESS) {
            kprintf(LOG_WARN, "ext2: failed to write superblock for device %pD: %d\n", mount->fs->device, ret);
            goto err_free;
        } else if (bytes != sizeof(mount->sb)) {
            ret = STATUS_CORRUPT_FS;
            goto err_free;
        }
    }

    mount->fs->root->id = EXT2_ROOT_INO;

    return STATUS_SUCCESS;

err_free:
    kfree(mount->group_table);
    kfree(mount);
    return ret;
}

static fs_type_t ext2_fs_type = {
    .name        = "ext2",
    .description = "Ext2/3/4",
    .probe       = ext2_probe,
    .mount       = ext2_mount,
};

MODULE_NAME("ext2");
MODULE_DESC("Ext2/3/4 filesystem support");
MODULE_FS_TYPE(ext2_fs_type);
