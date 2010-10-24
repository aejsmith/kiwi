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

/** Check if an object ACL entry is valid.
 * @param type		Type of entry.
 * @param value		Value for entry.
 * @param rights	Rights to give the entry.
 * @return		Whether the entry is valid. */
static bool object_acl_entry_valid(uint8_t type, int32_t value, object_rights_t rights) {
	if(type == ACL_ENTRY_USER || type == ACL_ENTRY_GROUP) {
		if(value < -1) {
			return false;
		}
	} else if(type == ACL_ENTRY_SESSION || type == ACL_ENTRY_CAPABILITY) {
		if(value < 0 || (type == ACL_ENTRY_CAPABILITY && value >= SECURITY_MAX_CAPS)) {
			return false;
		}
	} else if(type != ACL_ENTRY_OTHERS) {
		return false;
	}

	return true;
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
	if(!object_acl_entry_valid(type, value, rights)) {
		return;
	}

	/* Check if an identical entry already exists. */
	for(i = 0; i < acl->count; i++) {
		if(acl->entries[i].type == type && (type == ACL_ENTRY_OTHERS || acl->entries[i].value == value)) {
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

/** Calculate the rights that an ACL grants for a process.
 * @param object	Object the ACL belongs to.
 * @param acl		ACL to calculate from.
 * @param system	Whether the interpret the ACL as a system ACL.
 * @param process	Process to check (security lock held).
 * @param context	Security context of process.
 * @return		Set of rights that ACL grants the process. */
static object_rights_t object_acl_rights(object_t *object, object_acl_t *acl, bool system,
                                         process_t *process, security_context_t *context) {
	object_rights_t rights = 0, urights = 0, grights = 0, orights = 0;
	bool user = false, group = false;
	group_id_t gid;
	user_id_t uid;
	size_t i;

	/* Go through the entire ACL and calculate the rights allowed based on
	 * the process's user, group, session and capabilities, and for others.
	 * Any matching session and capability entries are always included in
	 * the calculated rights. */
	for(i = 0; i < acl->count; i++) {
		switch(acl->entries[i].type) {
		case ACL_ENTRY_USER:
			uid = (acl->entries[i].value < 0) ? object->uid : acl->entries[i].value;
			if(context->uid == uid) {
				urights |= acl->entries[i].rights;
				user = true;
			}
			break;
		case ACL_ENTRY_GROUP:
			gid = (acl->entries[i].value < 0) ? object->gid : acl->entries[i].value;
			if(security_context_has_group(context, gid)) {
				grights |= acl->entries[i].rights;
				group = true;
			}
			break;
		case ACL_ENTRY_OTHERS:
			orights |= acl->entries[i].rights;
			break;
		case ACL_ENTRY_SESSION:
			if(acl->entries[i].value == process->session->id) {
				rights |= acl->entries[i].rights;
			}
			break;
		case ACL_ENTRY_CAPABILITY:
			if(security_context_has_cap(context, acl->entries[i].value)) {
				rights |= acl->entries[i].rights;
			}
			break;
		}
	}

	if(system) {
		/* The system ACL uses all of the matching entries. */
		rights |= (urights | grights | orights);
	} else {
		/* If a user entry matched, we use that. Otherwise, if any group
		 * entries matched, we use the rights specified by all of them.
		 * Otherwise, we use the others entry. */
		if(user) {
			rights |= urights;
		} else if(group) {
			rights |= grights;
		} else {
			rights |= orights;
		}
	}

	return rights;
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
	object_acl_entry_t *entry;
	size_t i;

	/* Check if the IDs are valid. */
	if(security->uid < -1 || security->gid < -1) {
		return STATUS_INVALID_ARG;
	}

	/* If an ACL is provided, check for invalid entries and canonicalise it. */
	if(security->acl) {
		for(i = 0; i < security->acl->count; i++) {
			entry = &security->acl->entries[i];
			if(!object_acl_entry_valid(entry->type, entry->value, entry->rights)) {
				return STATUS_INVALID_ARG;
			}
		}

		object_acl_canonicalise(security->acl);
	}

	context = security_context_get(process);

	/* If specific user/group IDs are specified, check if we are allowed
	 * to use them. The CAP_CHANGE_OWNER capability allows the owners to
	 * be set to arbitrary values. */
	if(!security_context_has_cap(context, CAP_CHANGE_OWNER)) {
		if(security->uid >= 0 && security->uid != context->uid) {
			ret = STATUS_PERM_DENIED;
		} else if(security->gid >= 0 && !security_context_has_group(context, security->gid)) {
			ret = STATUS_PERM_DENIED;
		}
	}

	security_context_release(process);
	return ret;
}

/** Copy object security attributes from userspace.
 *
 * Copies an object security attributes structure from userspace memory,
 * canonicalises its ACL and validates it using object_security_validate().
 * Once the data copied is no longer required, the destination structure
 * should be passed to object_security_destroy().
 *
 * @param dest		Structure to copy to.
 * @param src		Userspace source pointer.
 *
 * @return		Status code describing result of the operation.
 */
status_t object_security_from_user(object_security_t *dest, const object_security_t *src) {
	object_acl_entry_t *entries = NULL;
	object_acl_t *acl = NULL;
	status_t ret;

	/* First copy the structure across. */
	ret = memcpy_from_user(dest, src, sizeof(*src));
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	/* If there is an ACL, copy it. */
	if(dest->acl) {
		acl = kmalloc(sizeof(*acl), MM_SLEEP);
		ret = memcpy_from_user(acl, dest->acl, sizeof(*acl));
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
		dest->acl = acl;

		/* Limit the maximum size of an ACL to prevent userspace from
		 * giving us a massive ACL. */
		if(acl->count > OBJECT_ACL_MAX) {
			ret = STATUS_TOO_LONG;
			goto fail;
		}

		/* If there are entries, copy them. */
		if(acl->count) {
			if(!acl->entries) {
				ret = STATUS_INVALID_ARG;
				goto fail;
			}

			entries = kmalloc(sizeof(*entries) * acl->count, MM_SLEEP);
			ret = memcpy_from_user(entries, acl->entries, sizeof(*entries) * acl->count);
			if(ret != STATUS_SUCCESS) {
				goto fail;
			}
			acl->entries = entries;
		} else {
			acl->entries = NULL;
		}
	}

	/* Validate the structure. */
	ret = object_security_validate(dest, NULL);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	return STATUS_SUCCESS;
fail:
	if(entries) { kfree(entries); }
	if(acl) { kfree(acl); }
	dest->acl = NULL;
	return ret;
}

/** Destroy an object security structure.
 * @param security	Structure to destroy. The structure itself will not be
 *			freed, only memory allocated for things within it. */
void object_security_destroy(object_security_t *security) {
	if(security->acl) {
		object_acl_destroy(security->acl);
		kfree(security->acl);
		security->acl = NULL;
	}
}

/** Calculate allowed rights for an object.
 * @param object	Object to calculate rights for.
 * @param process	Process to calculate rights for (if NULL, current
 *			process will be used).
 * @return		The set of rights that the process is allowed for the
 *			object. */
object_rights_t object_rights(object_t *object, process_t *process) {
	security_context_t *context;
	object_rights_t rights = 0;

	context = security_context_get(process);
	rights |= object_acl_rights(object, &object->uacl, false, process, context);
	rights |= object_acl_rights(object, &object->sacl, true, process, context);
	security_context_release(process);
	return rights;
}

/** Get the owning user/group of an object.
 * @param handle	Handle to object.
 * @param uidp		Where to store owning user ID.
 * @param gidp		Where to store owning group ID.
 * @return		Status code describing result of the operation. */
status_t sys_object_owner(handle_t handle, user_id_t *uidp, group_id_t *gidp) {
	object_handle_t *khandle;
	status_t ret;

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	rwlock_read_lock(&khandle->object->lock);

	if(uidp) {
		ret = memcpy_to_user(uidp, &khandle->object->uid, sizeof(*uidp));
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&khandle->object->lock);
			object_handle_release(khandle);
			return ret;
		}
	}
	if(gidp) {
		ret = memcpy_to_user(gidp, &khandle->object->gid, sizeof(*gidp));
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&khandle->object->lock);
			object_handle_release(khandle);
			return ret;
		}
	}

	rwlock_unlock(&khandle->object->lock);
	object_handle_release(khandle);
	return STATUS_SUCCESS;
}

/** Obtain a copy of an object's ACL.
 *
 * Obtains a copy of an object's access control list (ACL). It is recommended
 * that you use the object_security() wrapper function provided by libkernel
 * instead of this, as it will handle allocation of memory for the ACL for
 * you.
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

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
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
	ret = memcpy_to_user(&aclp->count, &khandle->object->uacl.count, sizeof(aclp->count));
	rwlock_unlock(&khandle->object->lock);
	object_handle_release(khandle);
	return ret;
}

/** Set security attributes for an object.
 *
 * Sets the security attributes (owning user/group and ACL) of an object. In
 * order to change the owning user/group IDs, the OBJECT_SET_OWNER right is
 * required on the handle. In order to set a new ACL, the OBJECT_SET_ACL right
 * is required.
 *
 * A process without the CAP_CHANGE_OWNER capability cannot set an owning user
 * ID different to its user ID, or set the owning group ID to that of a group
 * it does not belong to.
 *
 * The OBJECT_SET_ACL and OBJECT_SET_OWNER object rights are always granted to
 * an object's owning user, regardless of whether the ACL actually gives those
 * rights to the user. This is to prevent another user that the ACL grants
 * the OBJECT_SET_ACL right to from removing access to the object from the
 * owner.
 *
 * @param handle	Handle to object.
 * @param security	Security attributes to set. If the user ID is -1, it
 *			will not be changed. If the group ID is -1, it will not
 *			be changed. If the ACL pointer is NULL, the ACL will
 *			not be changed.
 *
 * @return		Status code describing result of the operation.
 */
status_t sys_object_set_security(handle_t handle, const object_security_t *security) {
	object_security_t ksecurity;
	object_handle_t *khandle;
	status_t ret;

	/* Copy the security structure from userspace. This checks that if a
	 * new user and group ID are specified the process is allowed to use
	 * them, and validates the new ACL. */
	ret = object_security_from_user(&ksecurity, security);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	ret = object_handle_lookup(NULL, handle, -1, 0, &khandle);
	if(ret != STATUS_SUCCESS) {
		object_security_destroy(&ksecurity);
		return ret;
	}

	/* Check if the necessary rights are set. */
	if(((ksecurity.uid >= 0 || ksecurity.gid >= 0) && !object_handle_rights(khandle, OBJECT_SET_OWNER)) ||
	   (ksecurity.acl && !object_handle_rights(khandle, OBJECT_SET_ACL))) {
		object_handle_release(khandle);
		object_security_destroy(&ksecurity);
		return STATUS_PERM_DENIED;
	}

	rwlock_write_lock(&khandle->object->lock);

	/* If the object type has a security validation function, call it. */
	if(khandle->object->type->set_security) {
		ret = khandle->object->type->set_security(khandle->object, &ksecurity);
		if(ret != STATUS_SUCCESS) {
			rwlock_unlock(&khandle->object->lock);
			object_handle_release(khandle);
			object_security_destroy(&ksecurity);
			return ret;
		}
	}

	/* Update the object. */
	if(ksecurity.uid >= 0) {
		khandle->object->uid = ksecurity.uid;
	}
	if(ksecurity.gid >= 0) {
		khandle->object->gid = ksecurity.gid;
	}
	if(ksecurity.acl) {
		khandle->object->uacl.entries = ksecurity.acl->entries;
		khandle->object->uacl.count = ksecurity.acl->count;
		ksecurity.acl->entries = NULL;
		ksecurity.acl->count = 0;
	}

	rwlock_unlock(&khandle->object->lock);
	object_handle_release(khandle);
	object_security_destroy(&ksecurity);
	return STATUS_SUCCESS;
}