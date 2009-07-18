/* Kiwi VFS system call interface
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		VFS system call interface.
 */

#ifndef __FS_SYSCALL_H
#define __FS_SYSCALL_H

#include <types.h>

/** Filesystem node information structure. */
typedef struct fs_info {
	int meow;
} fs_info_t;

/** Directory entry information structure. */
typedef struct fs_dir_entry {
	int meow;
} fs_dir_entry_t;

/** Structure containing arguments for sys_fs_file_map(). */
typedef struct fs_file_map_args {
	handle_t handle;		/**< Handle for file to map. */
	void *addr;			/**< Address to map at (if not AS_REGION_FIXED). */
	size_t size;			/**< Size of area to map (multiple of page size). */
	offset_t offset;		/**< Offset in the file to map from. */
	int flags;			/**< Flags controlling the mapping. */
	void **addrp;			/**< Where to store address mapped to. */
} fs_file_map_args_t;

extern int sys_fs_file_create(const char *path);
extern int sys_fs_file_path(char *buf, size_t size);
extern handle_t sys_fs_file_open(const char *path, int flags);
extern int sys_fs_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp);
extern int sys_fs_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp);
extern int sys_fs_file_truncate(handle_t handle, file_size_t size);
extern int sys_fs_file_seek(handle_t handle, int how, offset_t offset);
extern int sys_fs_file_map(fs_file_map_args_t *args);

extern int sys_fs_dir_create(const char *path);
extern handle_t sys_fs_dir_open(const char *path, int flags);
extern int sys_fs_dir_read(handle_t handle, fs_dir_entry_t *entry, size_t count);

extern int sys_fs_symlink_create(const char *name, const char *target);
extern int sys_fs_symlink_read(char *buf, size_t size);

extern int sys_fs_info(const char *path, handle_t handle, bool follow, fs_info_t *infop);
extern int sys_fs_link(const char *source, const char *dest);
extern int sys_fs_unlink(const char *path);
extern int sys_fs_rename(const char *source, const char *dest);
extern int sys_fs_getcwd(char *buf, size_t size);
extern int sys_fs_setcwd(const char *path);
extern int sys_fs_mount(const char *dev, const char *path, const char *type, int flags);
extern int sys_fs_unmount(const char *path);

#endif /* __FS_SYSCALL_H */
