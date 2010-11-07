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
 * @brief		POSIX change directory function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Set the current working directory.
 * @param path		Path to change to.
 * @return		0 on success, -1 on failure with errno set accordingly. */
int chdir(const char *path) {
	status_t ret;

	if(!path || !path[0]) {
		errno = ENOENT;
		return -1;
	}

	ret = fs_setcwd(path);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return ret;
}
