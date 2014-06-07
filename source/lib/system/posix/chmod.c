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
 * @brief		POSIX change file mode function.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <sys/stat.h>

#include <errno.h>
#include <stdlib.h>

#include "posix/posix.h"

/** Convert a mode to a set of rights.
 * @param mode		Mode to convert (the part of interest should be in the
 *			lowest 3 bits).
 * @return		Converted rights. */
static inline object_rights_t mode_to_rights(uint16_t mode) {
	object_rights_t rights = 0;

	if(mode & S_IROTH) {
		rights |= FILE_RIGHT_READ;
	}
	if(mode & S_IWOTH) {
		rights |= FILE_RIGHT_WRITE;
	}
	if(mode & S_IXOTH) {
		rights |= FILE_RIGHT_EXECUTE;
	}
	return rights;
}

/** Convert a POSIX file mode to a kernel ACL.
 * @param current	If not NULL, the current ACL. Entries not supported by
 *			POSIX (user and group entries for specific users) will
 *			be preserved in it.
 * @param mode		Mode to convert.
 * @return		Pointer to ACL on success, NULL on failure. */
object_acl_t *posix_mode_to_acl(object_acl_t *exist, mode_t mode) {
	object_rights_t rights;
	object_acl_t *acl;
	size_t i;

	if(exist) {
		acl = exist;

		/* Clear out any entries we're going to be modifying. */
		for(i = 0; i < acl->count; i++) {
			switch(acl->entries[i].type) {
			case ACL_ENTRY_USER:
				if(acl->entries[i].value < 0) {
					acl->entries[i].rights = 0;
				}
				break;
			case ACL_ENTRY_GROUP:
				if(acl->entries[i].value < 0) {
					acl->entries[i].rights = 0;
				}
				break;
			case ACL_ENTRY_OTHERS:
				acl->entries[i].rights = 0;
				break;
			}
		}
	} else {
		acl = malloc(sizeof(*acl));
		if(!acl) {
			return NULL;
		}

		object_acl_init(acl);
	}

	/* Add in the rights specified by the mode. */
	rights = mode_to_rights((mode & S_IRWXU) >> 6);
	if(object_acl_add_entry(acl, ACL_ENTRY_USER, -1, rights) != STATUS_SUCCESS) {
		goto fail;
	}
	rights = mode_to_rights((mode & S_IRWXG) >> 3);
	if(object_acl_add_entry(acl, ACL_ENTRY_GROUP, -1, rights) != STATUS_SUCCESS) {
		goto fail;
	}
	rights = mode_to_rights(mode & S_IRWXO);
	if(object_acl_add_entry(acl, ACL_ENTRY_OTHERS, 0, rights) != STATUS_SUCCESS) {
		goto fail;
	}

	return acl;
fail:
	if(!exist) {
		object_acl_destroy(acl);
		free(acl);
	}
	errno = ENOMEM;
	return NULL;
}

/** Change a file's mode.
 * @param path		Path to file.
 * @param mode		New mode for the file.
 * @return		0 on success, -1 on failure. */
int chmod(const char *path, mode_t mode) {
	object_security_t security;
	status_t ret;

	/* Get the current security attributes, as we want to preserve extra
	 * ACL entries. */
	ret = kern_fs_security(path, true, &security);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	/* Convert the mode to an ACL. */
	if(!posix_mode_to_acl(security.acl, mode)) {
		object_security_destroy(&security);
		return -1;
	}

	security.uid = -1;
	security.gid = -1;

	/* Set the new security attributes. */
	ret = kern_fs_set_security(path, true, &security);
	object_security_destroy(&security);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Change a file's mode.
 * @param fd		File descriptor to file.
 * @param mode		New mode for the file.
 * @return		0 on success, -1 on failure. */
int fchmod(int fd, mode_t mode) {
	object_security_t security;
	status_t ret;

	/* Get the current security attributes, as we want to preserve extra
	 * ACL entries. */
	ret = kern_object_security(fd, &security);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	/* Convert the mode to an ACL. */
	if(!posix_mode_to_acl(security.acl, mode)) {
		object_security_destroy(&security);
		return -1;
	}

	security.uid = -1;
	security.gid = -1;

	/* Set the new security attributes. */
	ret = kern_object_set_security(fd, &security);
	object_security_destroy(&security);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return 0;
}
