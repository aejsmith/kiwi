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
 * @brief		POSIX isatty() function.
 */

#include <kernel/object.h>

#include <errno.h>
#include <unistd.h>

/** Check whether a file descriptor refers to a TTY.
 * @param fd		File descriptor to check.
 * @return		1 if a TTY, 0 if not. */
int isatty(int fd) {
	switch(object_type(fd)) {
	case OBJECT_TYPE_DEVICE:
		return 1;
	case -1:
		errno = EBADF;
		return 0;
	default:
		errno = ENOTTY;
		return 0;
	}
}
