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
 * @brief		POSIX filesystem flush functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "libsystem.h"

/** Flush changes to a file to disk.
 * @param fd		Descriptor for file to flush. */
int fsync(int fd) {
	unsigned type;
	status_t ret;

	ret = kern_object_type(fd, &type);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	switch(type) {
	case OBJECT_TYPE_FILE:
		ret = kern_file_sync(fd);
		if(ret != STATUS_SUCCESS) {
			libsystem_status_to_errno(ret);
			return -1;
		}
		return 0;
	default:
		errno = EINVAL;
		return -1;
	}
}

/** Flush filesystem caches. */
void sync(void) {
	kern_fs_sync();
}
