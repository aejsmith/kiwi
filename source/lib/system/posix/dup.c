/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		POSIX file descriptor duplication functions.
 */

#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "libsystem.h"

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
	status_t ret;
	handle_t new;

	if(newfd < 0) {
		errno = EBADF;
		return -1;
	}

	ret = kern_handle_duplicate(fd, newfd, &new);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return new;
}
