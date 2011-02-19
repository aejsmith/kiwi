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
 * @brief		POSIX read symbolic link function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../libc.h"

/** Read the destination of a symbolic link.
 * @param path		Path to symbolc link.
 * @param buf		Buffer to read into.
 * @param size		Size of buffer.
 * @return		Number of bytes written to the buffer on success, or -1
 *			on failure. */
ssize_t readlink(const char *path, char *buf, size_t size) {
	char *tmp = NULL;
	file_info_t info;
	status_t ret;

	/* The kernel will not do anything if the buffer provided is too small,
	 * but we must return the truncated string if it is too small. Find out
	 * the link size, and allocate a large enough buffer if the given one
	 * is too small. */
	ret = kern_fs_info(path, false, &info);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	} else if(info.size >= size) {
		tmp = malloc(info.size + 1);
		if(!tmp) {
			return -1;
		}
	}

	ret = kern_symlink_read(path, (tmp) ? tmp : buf, info.size + 1);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	if(tmp) {
		memcpy(buf, tmp, size);
		free(buf);
		return size;
	} else {
		return info.size;
	}
}
