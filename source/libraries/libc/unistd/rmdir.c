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
 * @brief		POSIX directory removal function.
 */

#include <sys/stat.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

/** Remove a directory from the filesystem.
 * @param path		Path to directory to remove.
 * @return		0 on success, -1 on failure. */
int rmdir(const char *path) {
	const char *tmp;
	struct stat st;

	/* Must fail if the last part of the path is . or .. */
	tmp = strrchr(path, '/');
	if(!tmp) {
		tmp = path;
	}
	if(tmp[0] == '.' && (tmp[1] == 0 || (tmp[1] == '.' && tmp[2] == 0))) {
		errno = EINVAL;
		return -1;
	}

	/* Our unlink() implementation allows directory removal. However,
	 * rmdir() is supposed to return an error if not used on a directory.
	 * Therefore, we must use lstat() to determine whether or not the path
	 * is a directory first. */
	if(lstat(path, &st) != 0) {
		return -1;
	} else if(!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	return unlink(path);
}
