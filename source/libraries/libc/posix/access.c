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
 * @brief		POSIX file access check function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Check whether access to a file is allowed.
 * @param path		Path to file to check.
 * @param mode		Mode to check (F_OK, or any of the flags R_OK, W_OK and
 *			X_OK).
 * @return		0 if access is allowed, -1 if not with errno set
 *			accordingly. */
int access(const char *path, int mode) {
	object_rights_t rights = 0;
	file_info_t info;
	handle_t handle;
	status_t ret;

	ret = kern_fs_info(path, true, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	if(mode != F_OK) {
		if(mode & R_OK) {
			rights |= FILE_RIGHT_READ;
		}
		if(mode & W_OK) {
			rights |= FILE_RIGHT_WRITE;
		}
		if(mode & X_OK) {
			rights |= FILE_RIGHT_EXECUTE;
		}
	}

	switch(info.type) {
	case FILE_TYPE_REGULAR:
	case FILE_TYPE_DIR:
		ret = kern_file_open(path, rights, 0, 0, NULL, &handle);
		if(ret != STATUS_SUCCESS) {
			libc_status_to_errno(ret);
			return -1;
		}

		kern_handle_close(handle);
		break;
	default:
		/* Presume it's OK. */
		break;
	}

	return 0;
}
