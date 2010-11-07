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
 * @brief		POSIX symbolic link function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <unistd.h>

#include "../libc.h"

/** Create a symbolic link.
 * @param dest		Destination of the link.
 * @param path		Path name for the link.
 * @return		0 on success, -1 on failure. */
int symlink(const char *dest, const char *path) {
	status_t ret;

	ret = fs_symlink_create(path, dest);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
