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
 * @brief		POSIX file descriptor duplication functions.
 */

#include <fcntl.h>
#include <unistd.h>
#include "../libc.h"

/** Duplicate a file descriptor.
 * @param fd		File descriptor to duplicate.
 * @return		New FD, or -1 on failure. */
int dup(int fd) {
	return fcntl(fd, F_DUPFD, 0);
}

/** Duplicate a file descriptor.
 * @param fd		File descriptor to duplicate.
 * @param newfd		New file descriptor (if a file descriptor exists with
 *			this number, it will be closed).
 * @return		New FD, or -1 on failure. */
int dup2(int fd, int newfd) {
	libc_stub("dup2", false);
	return -1;
}
