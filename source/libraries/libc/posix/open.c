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
#include <stdarg.h>

#include "posix_priv.h"

/** Convert POSIX open() flags to kernel flags.
 * @param type		Node type.
 * @param oflag		POSIX open flags.
 * @param krightsp	Where to store kernel rights.
 * @param kflagsp	Where to store kernel flags.
 * @param kcreatep	Where to store kernel creation flags. */
static inline void convert_open_flags(fs_node_type_t type, int oflag, object_rights_t *krightsp,
                                      int *kflagsp, int *kcreatep) {
	object_rights_t krights = 0;
	int kflags = 0;

	krights |= ((oflag & O_RDONLY) ? FS_READ : 0);
	krights |= ((oflag & O_WRONLY) ? FS_WRITE : 0);

	kflags |= ((oflag & O_NONBLOCK) ? FS_NONBLOCK : 0);
	switch(type) {
	case FS_NODE_FILE:
		kflags |= ((oflag & O_APPEND) ? FS_FILE_APPEND : 0);
		break;
	default:
		break;
	}

	*krightsp = krights;
	*kflagsp = kflags;
	if(oflag & O_CREAT) {
		*kcreatep = (oflag & O_EXCL) ? FS_CREATE_ALWAYS : FS_CREATE;
	} else {
		*kcreatep = 0;
	}
}

/** Open a file or directory.
 * @todo		Convert mode to kernel ACL.
 * @param path		Path to file to open.
 * @param oflag		Flags controlling how to open the file.
 * @param ...		Mode to create the file with if O_CREAT is specified.
 * @return		File descriptor referring to file (positive value) on
 *			success, -1 on failure (errno will be set to the error
 *			reason). */
int open(const char *path, int oflag, ...) {
	object_security_t security = { -1, -1, NULL };
	object_rights_t rights;
	fs_node_type_t type;
	int kflags, kcreate;
	handle_t handle;
	fs_info_t info;
	uint16_t mode;
	status_t ret;
	va_list args;

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

	/* If O_CREAT is specified, we assume that we're going to be opening
	 * a file. Although POSIX doesn't specify anything about O_CREAT with
	 * a directory, Linux fails with EISDIR if O_CREAT is used with a
	 * directory that already exists. */
	if(oflag & O_CREAT) {
		type = FS_NODE_FILE;
	} else {
		/* Determine the filesystem entry type. */
		ret = fs_info(path, true, &info);
		if(ret != STATUS_SUCCESS) {
			libc_status_to_errno(ret);
			return -1;
		}

		type = info.type;
	}

	/* Convert the flags to kernel flags. */
	convert_open_flags(type, oflag, &rights, &kflags, &kcreate);

	/* Open according to the entry type. */
	switch(type) {
	case FS_NODE_FILE:
		if(oflag & O_DIRECTORY) {
			errno = ENOTDIR;
			return -1;
		}

		if(oflag & O_CREAT) {
			/* Obtain the creation mask. */
			va_start(args, oflag);
			mode = va_arg(args, mode_t);
			va_end(args);

			/* Apply the creation mode mask. */
			mode &= ~current_umask;

			/* Convert the mode to a kernel ACL. */
			security.acl = posix_mode_to_acl(NULL, mode);
			if(!security.acl) {
				return -1;
			}
		}

		/* Open the file, creating it if necessary. */
		ret = fs_file_open(path, rights, kflags, kcreate, &security, &handle);
		if(ret != STATUS_SUCCESS) {
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

		ret = fs_dir_open(path, rights, kflags, &handle);
		if(ret != STATUS_SUCCESS) {
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
