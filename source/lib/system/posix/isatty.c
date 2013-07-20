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
 * @brief		POSIX isatty() function.
 */

#include <kernel/object.h>

#include <errno.h>
#include <unistd.h>

/** Check whether a file descriptor refers to a TTY.
 * @todo		Check device type.
 * @param fd		File descriptor to check.
 * @return		1 if a TTY, 0 if not. */
int isatty(int fd) {
	switch(kern_object_type(fd)) {
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
