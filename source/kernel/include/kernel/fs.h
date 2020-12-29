/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Filesystem functions/definitions.
 */

#pragma once

#include <kernel/file.h>
#include <kernel/limits.h>

__KERNEL_EXTERN_C_BEGIN

/** Mount information structure. */
typedef struct mount_info {
    mount_id_t id;                      /**< Mount ID. */
    char type[FS_TYPE_MAX];             /**< Name of the filesystem type. */
    char path[FS_PATH_MAX];             /**< Path that mount is at. */
    char device[DEVICE_PATH_MAX];       /**< Device tree path of device. */
} mount_info_t;

/** Values for the create option of kern_fs_open(). */
#define FS_OPEN             0           /**< Open the file, do not create if non-existant. */
#define FS_CREATE           1           /**< Create the file if it doesn't exist. */
#define FS_MUST_CREATE      2           /**< If the file already exists, fail, else create it. */

/** Behaviour flags for kern_fs_mount(). */
#define FS_MOUNT_READ_ONLY  (1<<0)      /**< Mount filesystem as read-only. */

/** Behaviour flags for kern_fs_unmount(). */
#define FS_UNMOUNT_FORCE    (1<<0)      /**< Force unmounting even if unable to flush data. */

extern status_t kern_fs_open(
    const char *path, uint32_t access, uint32_t flags, unsigned create,
    handle_t *_handle);

extern status_t kern_fs_create_dir(const char *path);
extern status_t kern_fs_create_pipe(const char *path);
extern status_t kern_fs_create_symlink(const char *path, const char *target);

extern status_t kern_fs_read_symlink(const char *path, char *buf, size_t size);

extern status_t kern_fs_mount(
    const char *device, const char *path, const char *type, uint32_t flags,
    const char *opts);
extern status_t kern_fs_mount_info(mount_info_t *infos, size_t *_count);
extern status_t kern_fs_unmount(const char *path, unsigned flags);

extern status_t kern_fs_path(handle_t handle, char *buf, size_t size);
extern status_t kern_fs_curr_dir(char *buf, size_t size);
extern status_t kern_fs_set_curr_dir(const char *path);
extern status_t kern_fs_set_root_dir(const char *path);
extern status_t kern_fs_info(const char *path, bool follow, file_info_t *info);
extern status_t kern_fs_link(const char *path, const char *source);
extern status_t kern_fs_unlink(const char *path);
extern status_t kern_fs_rename(const char *source, const char *dest);
extern status_t kern_fs_sync(void);

__KERNEL_EXTERN_C_END
