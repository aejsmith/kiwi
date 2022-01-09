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
 * @brief               Ext2 block functions.
 */

#include <status.h>

#include "ext2.h"

/** Read a block from an Ext2 filesystem.
 * @param mount         Mount to read from.
 * @param buf           Buffer to read into.
 * @param num           Block number to read.
 * @return              Status code describing result of the operation. */
status_t ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t num) {
    if (num > mount->block_count) {
        /* This probably indicates some corrupt inode has an invalid block
         * reference. */
        kprintf(
            LOG_WARN, "ext2: %pD: attempted to read invalid block number %" PRIu32 "\n",
            mount->fs->device, num);

        return STATUS_CORRUPT_FS;
    }

    size_t bytes;
    status_t ret = file_read(mount->fs->handle, buf, mount->block_size, num * mount->block_size, &bytes);
    if (ret != STATUS_SUCCESS) {
        dprintf("ext2: %pD: failed to read block %" PRIu32 " (%d)\n", mount->fs->device, num, ret);
        return ret;
    } else if (bytes != mount->block_size) {
        return STATUS_CORRUPT_FS;
    }

    return STATUS_SUCCESS;
}

