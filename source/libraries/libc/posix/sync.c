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
 * @brief		POSIX filesystem flush functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Flush changes to a file to disk.
 * @param fd		Descriptor for file to flush. */
int fsync(int fd) {
	status_t ret;

	switch(object_type(fd)) {
	case OBJECT_TYPE_FILE:
		ret = kern_file_sync(fd);
		if(ret != STATUS_SUCCESS) {
			libc_status_to_errno(ret);
			return -1;
		}
		return 0;
	case -1:
		errno = EBADF;
		return -1;
	default:
		errno = EINVAL;
		return -1;
	}
}

/** Flush filesystem caches. */
void sync(void) {
	kern_fs_sync();
}
