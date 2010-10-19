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
 * @brief		Object ACL functions.
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
	} else {
		assert(!acl->entries);
	}
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
	khandle_t *khandle;
	object_acl_t uacl;
	size_t count;
	status_t ret;

	ret = handle_lookup(curr_proc, handle, -1, &khandle);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(!handle_rights(khandle, OBJECT_READ_SECURITY)) {
		handle_release(khandle);
		return ret;
	}

	ret = memcpy_from_user(&uacl, aclp, sizeof(uacl));
	if(ret != STATUS_SUCCESS) {
		handle_release(khandle);
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
				handle_release(khandle);
				return ret;
			}
		}
	}

	/* Copy back the number of ACL entries. */
	count = khandle->object->uacl.count;
	rwlock_unlock(&khandle->object->lock);
	ret = memcpy_to_user(&aclp->count, &count, sizeof(count));
	handle_release(khandle);
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
	return STATUS_NOT_IMPLEMENTED;
}
