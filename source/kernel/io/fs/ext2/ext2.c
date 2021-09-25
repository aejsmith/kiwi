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

#include <io/fs.h>

#include <module.h>
#include <status.h>

static bool ext2_probe(object_handle_t *device, const char *uuid) {
    kprintf(LOG_DEBUG, "ext2: probe\n");
    return false;
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
