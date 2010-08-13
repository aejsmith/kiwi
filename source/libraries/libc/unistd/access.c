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
 * @brief		POSIX file access check function.
 */

#include <sys/stat.h>

#include <errno.h>
#include <unistd.h>

/** Check whether access to a file is allowed.
 * @param path		Path to file to check.
 * @param mode		Mode to check (F_OK, or any of the flags R_OK, W_OK and
 *			X_OK).
 * @return		0 if access is allowed, -1 if not with errno set
 *			accordingly. */
int access(const char *path, int mode) {
	struct stat st;

	if(stat(path, &st) != 0) {
		return -1;
	}

	/* If just checking existance, don't need to do any more. */
	if(mode == F_OK) {
		return 0;
	}

	/* Check the mode in the stat structure.
	 * TODO: Not really correct, need to take user/group into account.
	 * TODO: Doesn't take into account whether the FS is mounted read-only. */
	if(mode & R_OK && !(st.st_mode & S_IRUSR || st.st_mode & S_IRGRP)) {
		errno = EACCES;
		return -1;
	}
	if(mode & W_OK && !(st.st_mode & S_IWUSR || st.st_mode & S_IWGRP)) {
		errno = EACCES;
		return -1;
	}
	if(mode & X_OK && !(st.st_mode & S_IXUSR || st.st_mode & S_IXGRP)) {
		errno = EACCES;
		return -1;
	}

	return 0;
}
