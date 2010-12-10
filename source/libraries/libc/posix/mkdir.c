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
 * @brief		POSIX make directory function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include "../libc.h"

/** Create a directory.
 * @todo		Convert mode to ACL.
 * @param path		Path to directory.
 * @param mode		Mode to create directory with.
 * @return		0 on success, -1 on failure. */
int mkdir(const char *path, mode_t mode) {
	status_t ret;

	ret = kern_dir_create(path, NULL);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
