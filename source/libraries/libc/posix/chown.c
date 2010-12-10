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
 * @brief		POSIX change owner function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <unistd.h>

#include "../libc.h"

/** Change the owner of a filesystem entry.
 * @param path		Path to entry.
 * @param uid		New user ID.
 * @param gid		New group ID.
 * @return		0 on success, -1 on failure. */
int chown(const char *path, uid_t uid, gid_t gid) {
	object_security_t security;
	status_t ret;

	security.uid = uid;
	security.gid = gid;
	security.acl = NULL;

	ret = kern_fs_set_security(path, true, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Change the owner of a filesystem entry.
 * @param path		Path to entry. If this refers to a symbolic link, it
 *			will not be dereferenced.
 * @param uid		New user ID.
 * @param gid		New group ID.
 * @return		0 on success, -1 on failure. */
int lchown(const char *path, uid_t uid, gid_t gid) {
	object_security_t security;
	status_t ret;

	security.uid = uid;
	security.gid = gid;
	security.acl = NULL;

	ret = kern_fs_set_security(path, false, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Change the owner of a filesystem entry.
 * @param fd		File descriptor to entry.
 * @param uid		New user ID.
 * @param gid		New group ID.
 * @return		0 on success, -1 on failure. */
int fchown(int fd, uid_t uid, gid_t gid) {
	object_security_t security;
	status_t ret;

	security.uid = uid;
	security.gid = gid;
	security.acl = NULL;

	ret = object_set_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
