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
