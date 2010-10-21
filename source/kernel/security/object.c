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
 * @brief		Object security functions.
 *
 * An object has two ACLs. The first is the user-specified ACL, which is the
 * primary ACL and can be modified by userspace. The other is the system ACL,
 * which is used internally by the kernel, primarily to assign rights based on
 * certain capabilities. This is done to prevent having to add special cases to
 * check for capabilities when opening objects. The system ACL cannot be
 * touched by userspace.
 */

#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/process.h>

#include <security/context.h>

#include <assert.h>
#include <object.h>
#include <status.h>

/** Initialise an ACL.
 * @param acl		ACL to initialise. */
void object_acl_init(object_acl_t *acl) {
	acl->entries = NULL;
	acl->count = 0;
}

/** Free memory used for an ACL.
 * @note		Structure itself is not freed. */
void object_acl_destroy(object_acl_t *acl) {
	if(acl->count) {
		assert(acl->entries);
		kfree(acl->entries);
		acl->entries = NULL;
		acl->count = 0;
	} else {
		assert(!acl->entries);
	}
}

/** Add an entry to an ACL keeping it in canonical form.
 * @param acl		ACL to add to.
 * @param type		Type of entry to add.
 * @param value		Value for entry. How this is interpreted depends on
 *			the entry type. For ACL_ENTRY_USER, it is a user ID,
 *			with -1 referring to the owning user. For
 *			ACL_ENTRY_GROUP, it is a group ID, with -1 referring to
 *			the owning group. For ACL_ENTRY_SESSION, it is a session
 *			ID. For ACL_ENTRY_CAPABILITY, it is a capability number.
 *			For ACL_ENTRY_OTHERS, it is ignored.
 * @param rights	Rights to give the entry. */
void object_acl_add_entry(object_acl_t *acl, uint8_t type, int32_t value, object_rights_t rights) {
	size_t i;

	/* Check that the type and value are valid. */
	if(type == ACL_ENTRY_USER || type == ACL_ENTRY_GROUP) {
		if(value < -1) {
			return;
		}
	} else if(type == ACL_ENTRY_SESSION || type == ACL_ENTRY_CAPABILITY) {
		if(value < 0) {
			return;
		}
	} else if(type != ACL_ENTRY_OTHERS) {
		return;
	}

	/* Check if an identical entry already exists. */
	for(i = 0; i < acl->count; i++) {
		if(acl->entries[i].type == type && acl->entries[i].value == value) {
			acl->entries[i].rights |= rights;
			return;
		}
	}

	/* Add a new entry. */
	acl->entries = krealloc(acl->entries, sizeof(acl->entries[0]) * (acl->count + 1), MM_SLEEP);
	acl->entries[acl->count  ].type = type;
	acl->entries[acl->count  ].value = value;
	acl->entries[acl->count++].rights = rights;
}

/** Canonicalise an object ACL.
 *
 * Converts an object ACL into canonical form. An ACL is considered to be in
 * canonical form if there are no duplicate entries (entries with the same type
 * and referring to the same thing, e.g. multiple entries for one user).
 * Duplicate entries are merged together. Invalid entries (entries with an
 * invalid type or value) are also removed.
 *
 * @param acl		ACL to canonicalise.
 */
void object_acl_canonicalise(object_acl_t *acl) {
	object_acl_t copy;
	size_t i;

	/* Since object_acl_add_entry() maintains canonical form, just build
	 * a new ACL using the provided ACL. */
	object_acl_init(&copy);
	for(i = 0; i < acl->count; i++) {
		object_acl_add_entry(&copy, acl->entries[i].type, acl->entries[i].value,
		                     acl->entries[i].rights);
	}

	/* Replace the old ACL with the new one. */
	object_acl_destroy(acl);
	acl->entries = copy.entries;
	acl->count = copy.count;
}

/** Validate object security attributes.
 *
 * Validates an object security attributes structure against a process' security
 * context to check if the user and group the structure specifies are allowed
 * by the context. If the context has the CAP_CHANGE_OWNER capability, any
 * user/group ID is allowed. Otherwise, only the context's user ID and the ID
 * of any groups it is in are allowed.
 *
 * @param security	Security attributes to validate. If an ACL is specified
 *			by the structure, it will be canonicalised (see
 *			object_acl_canonicalise()).
 * @param process	Process to validate against. If NULL, the current
 *			process will be used.
 *
 * @return		STATUS_SUCCESS if passed validation, other code if not.
 */
status_t object_security_validate(object_security_t *security, process_t *process) {
	status_t ret = STATUS_SUCCESS;
	security_context_t *context;
	size_t i;

	/* Check if the IDs are valid. */
	if(security->uid < -1 || security->gid < -1) {
		return STATUS_INVALID_ARG;
	}

	/* Canonicalise the ACL (if provided). */
	if(security->acl) {
		object_acl_canonicalise(security->acl);
	}

	context = security_context_get(process);

	/* If specific user/group IDs are specified, check if we are allowed
	 * to use them. The CAP_CHANGE_OWNER capability allows the owners to
	 * be set to arbitrary values. */
	if(!security_context_has_cap(context, CAP_CHANGE_OWNER)) {
		if(security->uid >= 0 && security->uid != context->uid) {
			ret = STATUS_PERM_DENIED;
		} else if(security->gid >= 0) {
			ret = STATUS_PERM_DENIED;

			for(i = 0; i < SECURITY_MAX_GROUPS; i++) {
				if(context->groups[i] < 0) {
					break;
				} else if(context->groups[i] == security->gid) {
					ret = STATUS_SUCCESS;
					break;
				}
			}
		}
	}

	security_context_release(process);
	return ret;
}

/** Copy object security attributes from userspace.
 *
 * Copies an object security attributes structure from userspace memory,
 * canonicalises it and validates it using object_security_validate().
 * Once the data copied is no longer required, the destination structure
 * should be passed to object_security_destroy().
 *
 * @param dest		Structure to copy to.
 * @param src		Userspace source pointer.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_security_from_user(object_security_t *dest, const object_security_t *src) {
	// should return error if acl invalid. (in validate above)
	return STATUS_NOT_IMPLEMENTED;
}

/** Destroy an object security structure.
 * @param security	Structure to destroy. The structure itself will not be
 *			freed, only memory allocated for things within it. */
void object_security_destroy(object_security_t *security) {
	if(security->acl) {
		object_acl_destroy(security->acl);
		kfree(security->acl);
	}
}

/** Get the owning user/group of an object.
 *
 * Gets the owning user/group of an object. The handle must have the
 * OBJECT_READ_SECURITY right.
 *
 * @param handle	Handle to object.
 * @param uidp		Where to store owning user ID.
 * @param gidp		Where to store owning group ID.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_object_owner(handle_t handle, user_id_t *uidp, group_id_t *gidp) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Set the owning user/group of an object.
 *
 * Sets the owning user/group of an object. The handle must have the
 * OBJECT_SET_OWNER right.
 *
 * @param handle	Handle to object to set owner of.
 * @param uid		New owning user ID (or -1 to not change).
 * @param gid		New owning group ID (or -1 to not change).
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_object_set_owner(handle_t handle, user_id_t uid, group_id_t gid) {
	return STATUS_NOT_IMPLEMENTED;
}

/** Obtain a copy of an object's ACL.
 *
 * Obtains a copy of an object's access control list (ACL). The handle must
 * have the OBJECT_READ_SECURITY right.
 *
 * @param handle	Handle to object to get ACL of.
 * @param acl		Where to store ACL. The structure referred to by this
 *			pointer must be initialised prior to calling the
 *			function. If the entries pointer in the structure is
 *			NULL, then the function will store the number of
 *			entries in the ACL in the count entry and do nothing
 *			else. Otherwise, at most the number of entries specified
 *			by the count entry will be copied to the entries
 *			array, and the count will be updated to give the actual
 *			number of entries in the ACL.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_object_acl(handle_t handle, object_acl_t *aclp) {
	object_handle_t *khandle;
	object_acl_t uacl;
	size_t count;
	status_t ret;

	ret = object_handle_lookup(curr_proc, handle, -1, OBJECT_READ_SECURITY, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = memcpy_from_user(&uacl, aclp, sizeof(uacl));
	if(ret != STATUS_SUCCESS) {
		object_handle_release(khandle);
		return ret;
	}

	rwlock_read_lock(&khandle->object->lock);

	/* If entries pointer is NULL, the caller wants us to give the number
	 * of entries in the ACL. Otherwise, copy at most the number of entries
	 * specified. */
	if(uacl.entries) {
		count = MIN(uacl.count, khandle->object->uacl.count);
		if(count) {
			ret = memcpy_to_user(uacl.entries,
			                     khandle->object->uacl.entries,
			                     sizeof(*uacl.entries) * count);
			if(ret != STATUS_SUCCESS) {
				rwlock_unlock(&khandle->object->lock);
				object_handle_release(khandle);
				return ret;
			}
		}
	}

	/* Copy back the number of ACL entries. */
	count = khandle->object->uacl.count;
	rwlock_unlock(&khandle->object->lock);
	ret = memcpy_to_user(&aclp->count, &count, sizeof(count));
	object_handle_release(khandle);
	return ret;
}

/** Set the ACL of an object.
 *
 * Sets the access control list for an object. The handle must have the
 * OBJECT_SET_ACL right. Entries of the same type in the ACL will be cleaned in the provided ACL
 *
 * @param handle	Handle to object.
 * @param acl		ACL to set.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_object_set_acl(handle_t handle, const object_acl_t *acl) {
	// should canonicalise, return error if any invalid entries, fix doc.
	// remove me, replace with object_set_security. replace -1 and -1
	return STATUS_NOT_IMPLEMENTED;
}
