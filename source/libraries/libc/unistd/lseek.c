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
 * @brief		POSIX seek function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Change a file descriptor's offset.
 *
 * Changes the offset of a file descriptor according to the specified action.
 * If the action is SEEK_SET, the offset will be set to the exact value given.
 * If it is SEEK_CUR, the offset will be set to the current offset plus the
 * value given. If it is SEEK_END, the offset will be set to the end of the
 * file plus the specified number of bytes.
 *
 * @param fd		File descriptor to change offset of.
 * @param off		Offset value (used according to action).
 * @param act		Action to perform.
 *
 * @return		New file offset, or -1 on failure.
 */
off_t lseek(int fd, off_t off, int act) {
	offset_t new;
	status_t ret;
	int kact;

	switch(act) {
	case SEEK_SET: kact = FS_SEEK_SET; break;
	case SEEK_CUR: kact = FS_SEEK_ADD; break;
	case SEEK_END: kact = FS_SEEK_END; break;
	default:
		errno = EINVAL;
		return -1;
	}

	ret = fs_handle_seek(fd, kact, off, &new);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return new;
}
