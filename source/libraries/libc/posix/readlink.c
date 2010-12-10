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
 * @brief		POSIX read symbolic link function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libc.h"

/** Read the destination of a symbolic link.
 * @param path		Path to symbolc link.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer.
 * @return		Number of bytes written to the buffer on success, or -1
 *			on failure. */
ssize_t readlink(const char *path, char *buf, size_t size) {
	char *tmp = NULL;
	file_info_t info;
	status_t ret;

	/* The kernel will not do anything if the buffer provided is too small,
	 * but we must return the truncated string if it is too small. Find out
	 * the link size, and allocate a large enough buffer if the given one
	 * is too small. */
	ret = kern_fs_info(path, false, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	} else if(info.size >= size) {
		tmp = malloc(info.size + 1);
		if(!tmp) {
			return -1;
		}
	}

	ret = kern_symlink_read(path, (tmp) ? tmp : buf, info.size + 1);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	if(tmp) {
		memcpy(buf, tmp, size);
		free(buf);
		return size;
	} else {
		return info.size;
	}
}
