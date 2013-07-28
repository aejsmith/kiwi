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
 * @brief		POSIX change owner function.
 */

#include <kernel/fs.h>
#include <kernel/status.h>

#include <unistd.h>

#include "libsystem.h"

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
		libsystem_status_to_errno(ret);
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
		libsystem_status_to_errno(ret);
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

	ret = kern_object_set_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return 0;
}
