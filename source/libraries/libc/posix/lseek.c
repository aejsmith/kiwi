/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
	case SEEK_SET: kact = FILE_SEEK_SET; break;
	case SEEK_CUR: kact = FILE_SEEK_ADD; break;
	case SEEK_END: kact = FILE_SEEK_END; break;
	default:
		errno = EINVAL;
		return -1;
	}

	ret = kern_file_seek(fd, kact, off, &new);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return new;
}
