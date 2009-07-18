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

#include <fs/syscall.h>
#include <fs/vfs.h>

#include <errors.h>

#if 0
# pragma mark File-related calls.
#endif

int sys_fs_file_create(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_path(char *buf, size_t size) {
	return -ERR_NOT_IMPLEMENTED;
}

handle_t sys_fs_file_open(const char *path, int flags) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_read(handle_t handle, void *buf, size_t count, size_t *bytesp) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_write(handle_t handle, const void *buf, size_t count, size_t *bytesp) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_truncate(handle_t handle, file_size_t size) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_seek(handle_t handle, int how, offset_t offset) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_file_map(fs_file_map_args_t *args) {
	return -ERR_NOT_IMPLEMENTED;
}

#if 0
# pragma mark Directory-related calls.
#endif

int sys_fs_dir_create(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}

handle_t sys_fs_dir_open(const char *path, int flags) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_dir_read(handle_t handle, fs_dir_entry_t *entry, size_t count) {
	return -ERR_NOT_IMPLEMENTED;
}

#if 0
# pragma mark Symlink-related calls.
#endif

int sys_fs_symlink_create(const char *name, const char *target) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_symlink_read(char *buf, size_t size) {
	return -ERR_NOT_IMPLEMENTED;
}

#if 0
# pragma mark Other FS calls.
#endif

int sys_fs_info(const char *path, handle_t handle, bool follow, fs_info_t *infop) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_link(const char *source, const char *dest) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_unlink(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_rename(const char *source, const char *dest) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_getcwd(char *buf, size_t size) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_setcwd(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_mount(const char *dev, const char *path, const char *type, int flags) {
	return -ERR_NOT_IMPLEMENTED;
}

int sys_fs_unmount(const char *path) {
	return -ERR_NOT_IMPLEMENTED;
}
