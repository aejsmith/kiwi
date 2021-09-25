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

#include <module.h>
#include <status.h>

#include "ext2.h"

static bool ext2_probe(device_t *device, object_handle_t *handle, const char *uuid) {
    ext2_superblock_t *sb __cleanup_kfree = kmalloc(sizeof(ext2_superblock_t), MM_KERNEL);

    size_t bytes;
    if (file_read(handle, sb, sizeof(*sb), 1024, &bytes) != STATUS_SUCCESS) {
        return false;
    } else if (bytes != sizeof(ext2_superblock_t) || le16_to_cpu(sb->s_magic) != EXT2_MAGIC) {
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
    uint32_t incompat = le32_to_cpu(sb->s_feature_incompat);
    if (incompat & ~(uint32_t)EXT2_FEATURE_INCOMPAT_SUPP) {
        kprintf(
            LOG_NOTICE, "ext2: device %pD has unsupported incompatible features 0x%x\n",
            device, sb->s_feature_incompat);

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

static status_t ext2_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
    kprintf(LOG_DEBUG, "ext2: mount\n");
    return STATUS_NOT_IMPLEMENTED;
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
