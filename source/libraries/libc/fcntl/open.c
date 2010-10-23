/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		POSIX file open function.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>

#include "../libc.h"

/** Open a file or directory.
 * @todo		Handle the mode argument.
 * @param path		Path to file to open.
 * @param oflag		Flags controlling how to open the file.
 * @param ...		Mode to create the file with if O_CREAT is specified.
 * @return		File descriptor referring to file (positive value) on
 *			success, -1 on failure (errno will be set to the error
 *			reason). */
int open(const char *path, int oflag, ...) {
	object_rights_t rights;
	fs_node_type_t type;
	handle_t handle;
	fs_info_t info;
	status_t ret;
	int kflag;

	/* Check whether the arguments are valid. I'm not sure if the second
	 * check is correct, POSIX doesn't say anything about O_CREAT with
	 * O_DIRECTORY. */
	if(!(oflag & O_RDWR) || (oflag & O_EXCL && !(oflag & O_CREAT))) {
		errno = EINVAL;
		return -1;
	} else if(oflag & O_CREAT && oflag & O_DIRECTORY) {
		errno = EINVAL;
		return -1;
	} else if(!(oflag & O_WRONLY) && oflag & O_TRUNC) {
		errno = EACCES;
		return -1;
	}
retry:
	/* Determine the filesystem entry type. */
	ret = fs_info(path, true, &info);
	if(ret == STATUS_SUCCESS) {
		if(oflag & O_EXCL) {
			errno = EEXIST;
			return -1;
		}
		type = info.type;
	} else if(ret == STATUS_NOT_FOUND && (oflag & O_CREAT)) {
		/* File does not exist. Attempt to create it. */
		ret = fs_file_create(path);
		if(ret != STATUS_SUCCESS) {
			if(ret == STATUS_ALREADY_EXISTS) {
				goto retry;
			}
			libc_status_to_errno(ret);
			return -1;
		}
		type = FS_NODE_FILE;
	} else {
		libc_status_to_errno(ret);
		return -1;
	}

	/* Convert the flags to kernel flags. */
	rights = 0;
	rights |= ((oflag & O_RDONLY) ? FS_READ : 0);
	rights |= ((oflag & O_WRONLY) ? FS_WRITE : 0);
	kflag = 0;
	kflag |= ((oflag & O_NONBLOCK) ? FS_NONBLOCK : 0);

	/* Open the entry according to the entry type. */
	switch(type) {
	case FS_NODE_FILE:
		if(oflag & O_DIRECTORY) {
			errno = ENOTDIR;
			return -1;
		}

		/* Convert the flags to kernel flags. */
		kflag |= ((oflag & O_APPEND) ? FS_FILE_APPEND : 0);

		/* Open the file. */
		ret = fs_file_open(path, rights, kflag, &handle);
		if(ret != STATUS_SUCCESS) {
			if(ret == STATUS_NOT_FOUND) {
				goto retry;
			}
			libc_status_to_errno(ret);
			return -1;
		}

		/* Truncate the file if requested. */
		if(oflag & O_TRUNC) {
			ret = fs_file_resize(handle, 0);
			if(ret != STATUS_SUCCESS) {
				handle_close(handle);
				libc_status_to_errno(ret);
				return -1;
			}
		}
		break;
	case FS_NODE_DIR:
		if(oflag & O_WRONLY || oflag & O_TRUNC) {
			errno = EISDIR;
			return -1;
		}

		ret = fs_dir_open(path, rights, kflag, &handle);
		if(ret != STATUS_SUCCESS) {
			if(ret == STATUS_NOT_FOUND) {
				goto retry;
			}
			libc_status_to_errno(ret);
			return -1;
		}
		break;
	default:
		errno = ENOTSUP;
		return -1;
	}

	/* Mark the handle as inheritable if not opening with O_CLOEXEC. */
	if(!(oflag & O_CLOEXEC)) {
		handle_set_flags(handle, HANDLE_INHERITABLE);
	}

	return (int)handle;
}

/** Open and possibly create a file.
 *
 * Opens a file, creating it if it does not exist. If it does exist, it will be
 * truncated to zero length.
 *
 * @param path		Path to file.
 * @param mode		Mode to create file with if it doesn't exist.
 *
 * @return		File descriptor referring to file (positive value) on
 *			success, -1 on failure (errno will be set to the error
 *			reason).
 */
int creat(const char *path, mode_t mode) {
	return open(path, O_WRONLY | O_CREAT | O_TRUNC, mode);
}
