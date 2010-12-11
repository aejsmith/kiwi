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
 * @brief		Object functions.
 */

#include <kernel/object.h>
#include <kernel/status.h>

#include <stdlib.h>

#include "libkernel.h"

extern status_t _kern_object_security(handle_t handle, user_id_t *uidp, group_id_t *gidp,
                                      object_acl_t *aclp);

/** Obtain object security attributes.
 * @param handle	Handle to object to get attributes for.
 * @param securityp	Security structure to fill in. Memory is allocated for
 *			data within this structure, which means it must be
 *			freed with object_security_destroy() once it is no
 *			longer needed.
 * @return		Status code describing result of the operation. */
__export status_t kern_object_security(handle_t handle, object_security_t *securityp) {
	status_t ret;

	securityp->acl = malloc(sizeof(object_acl_t));
	if(!securityp->acl) {
		return STATUS_NO_MEMORY;
	}

	/* Call with a NULL entries pointer in order to get the size of the ACL.
	 * TODO: What if the ACL is changed between the two calls? */
	securityp->acl->entries = NULL;
	ret = _kern_object_security(handle, &securityp->uid, &securityp->gid, securityp->acl);
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
	ret = _kern_object_security(handle, NULL, NULL, securityp->acl);
	if(ret != STATUS_SUCCESS) {
		free(securityp->acl->entries);
		free(securityp->acl);
		return ret;
	}

	return STATUS_SUCCESS;
}

/** Get the ACL from an object security structure.
 * @param security	Structure to get from.
 * @return		Pointer to ACL, or NULL if failed to allocate one. */
__export object_acl_t *object_security_acl(object_security_t *security) {
	if(!security->acl) {
		security->acl = malloc(sizeof(object_acl_t));
		if(!security->acl) {
			return NULL;
		}
	}

	return security->acl;
}

/** Free memory allocated for an object security structure.
 * @note		Structure itself is not freed.
 * @param security	Structure to free data for. */
__export void object_security_destroy(object_security_t *security) {
	if(security->acl) {
		if(security->acl->entries) {
			free(security->acl->entries);
			security->acl->entries = NULL;
		}
		free(security->acl);
		security->acl = NULL;
	}
}

/** Initialise an ACL.
 * @param acl		ACL to initialise. */
__export void object_acl_init(object_acl_t *acl) {
	acl->entries = NULL;
	acl->count = 0;
}

/** Free memory used for an ACL.
 * @note		Structure itself is not freed. */
__export void object_acl_destroy(object_acl_t *acl) {
	if(acl->count) {
		free(acl->entries);
		acl->entries = NULL;
		acl->count = 0;
	}
}

/** Add an entry to an ACL.
 * @param acl		ACL to add to.
 * @param type		Type of entry to add.
 * @param value		Value for entry. How this is interpreted depends on
 *			the entry type. For ACL_ENTRY_USER, it is a user ID,
 *			with -1 referring to the owning user. For
 *			ACL_ENTRY_GROUP, it is a group ID, with -1 referring to
 *			the owning group. For ACL_ENTRY_SESSION, it is a session
 *			ID. For ACL_ENTRY_CAPABILITY, it is a capability number.
 *			For ACL_ENTRY_OTHERS, it is ignored.
 * @param rights	Rights to give the entry.
 * @return		Status code describing result of the operation. Note
 *			that this function does not check for invalid entries,
 *			it will only return an error if memory allocation fails.
 *			Invalid entries will be picked up by the kernel when
 *			the ACL is given to it. */
__export status_t object_acl_add_entry(object_acl_t *acl, uint8_t type, int32_t value,
                                       object_rights_t rights) {
	object_acl_entry_t *tmp;
	size_t i;

	/* Check if an identical entry already exists. */
	for(i = 0; i < acl->count; i++) {
		if(acl->entries[i].type == type && (type == ACL_ENTRY_OTHERS || acl->entries[i].value == value)) {
			acl->entries[i].rights |= rights;
			return STATUS_SUCCESS;
		}
	}

	/* Add a new entry. */
	tmp = realloc(acl->entries, sizeof(acl->entries[0]) * (acl->count + 1));
	if(!tmp) {
		return STATUS_NO_MEMORY;
	}
	acl->entries = tmp;
	acl->entries[acl->count  ].type = type;
	acl->entries[acl->count  ].value = value;
	acl->entries[acl->count++].rights = rights;
	return STATUS_SUCCESS;
}
