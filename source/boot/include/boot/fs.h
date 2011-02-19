/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Filesystem functions.
 */

#ifndef __BOOT_FS_H
#define __BOOT_FS_H

#include <boot/disk.h>

struct fs_mount;
struct fs_handle;

/** Type of a fs_dir_read() callback.
 * @param name		Name of the entry.
 * @param handle	Handle to entry.
 * @param data		Data argument passed to fs_dir_read().
 * @return		Whether to continue iteration. */
typedef bool (*fs_dir_read_cb_t)(const char *name, struct fs_handle *handle, void *arg);

/** Structure containing operations for a filesystem. */
typedef struct fs_type {
	/** Mount an instance of this filesystem.
	 * @param mount		Mount structure to fill in.
	 * @return		Whether succeeded in mounting. */
	bool (*mount)(struct fs_mount *mount);

	/** Open a file/directory on the filesystem.
	 * @note		If not provided, a generic implementation will
	 *			be used that uses read_dir().
	 * @param mount		Mount to open from.
	 * @param path		Path to file/directory to open.
	 * @return		Pointer to handle on success, NULL on failure. */
	struct fs_handle *(*open)(struct fs_mount *mount, const char *path);

	/** Close a handle.
	 * @param handle	Handle to close. */
	void (*close)(struct fs_handle *handle);

	/** Read from a file.
	 * @param handle	Handle to the file.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset into the file.
	 * @return		Whether read successfully. */
	bool (*read)(struct fs_handle *handle, void *buf, size_t count, offset_t offset);

	/** Get the size of a file.
	 * @param handle	Handle to the file.
	 * @return		Size of the file. */
	offset_t (*size)(struct fs_handle *handle);

	/** Read directory entries.
	 * @param handle	Handle to directory.
	 * @param cb		Callback to call on each entry.
	 * @param arg		Data to pass to callback.
	 * @return		Whether read successfully. */
	bool (*read_dir)(struct fs_handle *handle, fs_dir_read_cb_t cb, void *arg);
} fs_type_t;

/** Structure representing a mounted filesystem. */
typedef struct fs_mount {
	fs_type_t *type;		/**< Type structure for the filesystem. */
	struct fs_handle *root;		/**< Handle to root of FS (not needed if open() implemented). */
	void *data;			/**< Implementation-specific data pointer. */
	disk_t *disk;			/**< Disk that the filesystem resides on. */
	char *label;			/**< Label of the filesystem. */
	char *uuid;			/**< UUID of the filesystem. */
} fs_mount_t;

/** Structure representing a handle to a filesystem entry. */
typedef struct fs_handle {
	fs_mount_t *mount;		/**< Mount the entry is on. */
	bool directory;			/**< Whether the entry is a directory. */
	void *data;			/**< Implementation-specific data pointer. */
	int count;			/**< Reference count. */
} fs_handle_t;

extern fs_type_t ext2_fs_type;
extern fs_type_t iso9660_fs_type;

extern fs_handle_t *fs_handle_create(fs_mount_t *mount, bool directory, void *data);
extern fs_mount_t *fs_probe(disk_t *disk);
extern fs_handle_t *fs_open(fs_mount_t *mount, const char *path);
extern void fs_close(fs_handle_t *handle);
extern bool fs_file_read(fs_handle_t *handle, void *buf, size_t count, offset_t offset);
extern offset_t fs_file_size(fs_handle_t *handle);
extern bool fs_dir_read(fs_handle_t *handle, fs_dir_read_cb_t cb, void *arg);

#endif /* __BOOT_FS_H */
