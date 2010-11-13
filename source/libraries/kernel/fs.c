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
 * @brief		Filesystem functions.
 */

#include <kernel/fs.h>
#include <kernel/object.h>
#include <kernel/status.h>

#include <stdlib.h>

#include "libkernel.h"

extern status_t _fs_security(const char *path, bool follow, user_id_t *uidp,
                             group_id_t *gidp, object_acl_t *aclp);

/** Obtain security attributes for a filesystem entry.
 * @param path		Path to entry to get security attributes for.
 * @param follow	Whether to follow if last path component is a symbolic
 *			link.
 * @param securityp	Security structure to fill in. Memory is allocated for
 *			data within this structure, which means it must be
 *			freed with object_security_destroy() once it is no
 *			longer needed.
 * @return		Status code describing result of the operation. */
__export status_t fs_security(const char *path, bool follow, object_security_t *securityp) {
	status_t ret;

	securityp->acl = malloc(sizeof(object_acl_t));
	if(!securityp->acl) {
		return STATUS_NO_MEMORY;
	}

	/* Call with a NULL entries pointer in order to get the size of the ACL.
	 * TODO: What if the ACL is changed between the two calls? */
	securityp->acl->entries = NULL;
	ret = _fs_security(path, follow, &securityp->uid, &securityp->gid, securityp->acl);
	if(ret != STATUS_SUCCESS) {
		free(securityp->acl);
		return ret;
	}

	securityp->acl->entries = malloc(sizeof(object_acl_entry_t) * securityp->acl->count);
	if(!securityp->acl->entries) {
		free(securityp->acl);
		return STATUS_NO_MEMORY;
	}

	/* Get the ACL entries. */
	ret = _fs_security(path, follow, NULL, NULL, securityp->acl);
	if(ret != STATUS_SUCCESS) {
		free(securityp->acl->entries);
		free(securityp->acl);
		return ret;
	}

	return STATUS_SUCCESS;
}
