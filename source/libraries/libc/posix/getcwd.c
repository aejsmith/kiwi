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
 * @brief		POSIX get working directory function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <errno.h>
#include <limits.h>
#include <unistd.h>

#include "../libc.h"

/** Get the path to the current working directory.
 * @param buf		Buffer to place path string in.
 * @param size		Size of the buffer. If it is too small, errno will be
 *			set to ERANGE.
 * @return		Pointer to buffer or NULL on failure. */
char *getcwd(char *buf, size_t size) {
	status_t ret;

	if(!size || !buf) {
		errno = EINVAL;
		return NULL;
	}

	ret = fs_getcwd(buf, size);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return NULL;
	}

	return buf;
}

/** Get the path to the current working directory.
 * @param buf		Buffer to place path string in. Must be at least
 *			PATH_MAX bytes long.
 * @return		Pointer to buffer or NULL on failure. */
char *getwd(char *buf) {
	if(!getcwd(buf, PATH_MAX)) {
		if(errno == ERANGE) {
			errno = ENAMETOOLONG;
		}
		return NULL;
	}
	return buf;
}
