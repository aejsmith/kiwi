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
 * @brief		Security functions.
 */

#ifndef __KERNEL_SECURITY_H
#define __KERNEL_SECURITY_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum number of groups that a security context can have. */
#define SECURITY_MAX_GROUPS		32

/** Maximum number of capabilities. */
#define SECURITY_MAX_CAPS		128

/** Definitions of capabilities.
 * @note		Update security_init() when adding capabilities. */
#define CAP_SECURITY_AUTHORITY		0	/**< Ability to set any process' security context. */
#define CAP_CREATE_SESSION		1	/**< Ability to create new sessions. */
#define CAP_CHANGE_IDENTITY		2	/**< Ability to change user/group IDs. */
#define CAP_MODULE			3	/**< Ability to load/unload kernel modules. */
#define CAP_FS_ADMIN			4	/**< Ability to bypass access checks on the filesystem. */
#define CAP_FS_SETROOT			5	/**< Ability to use the fs_setroot() system call. */
#define CAP_FS_MOUNT			6	/**< Ability to mount/unmount filesystems. */
#define CAP_CHANGE_OWNER		7	/**< Ability to set object user/group to arbitrary IDs. */
#define CAP_FATAL			8	/**< Ability to halt the kernel with a fatal error. */
#define CAP_SHUTDOWN			9	/**< Ability to shut down the system. */

/** Structure defining the security context for a process/thread.
 * @note		Should be modified using the security_context_*()
 *			functions. */
typedef struct security_context {
	user_id_t uid;			/**< User ID. */

	/** Groups that the process belongs to (all unused entries negative).
	 * @note		Primary group is the first used entry in the
	 *			structure. This group is used as the default
	 *			owning group when one is not specified during
	 *			object creation. The order of other groups is
	 *			irrelevant. */
	group_id_t groups[SECURITY_MAX_GROUPS];
	
	/** Capabilities for the process. */
	uint64_t caps[SECURITY_MAX_CAPS / 64];
} security_context_t;

/** Initialise a security context.
 * @param context	Context to intialise. */
static inline void security_context_init(security_context_t *context) {
	size_t i;

	context->uid = 0;

	for(i = 0; i < SECURITY_MAX_GROUPS; i++) {
		context->groups[i] = -1;
	}
	for(i = 0; i < (SECURITY_MAX_CAPS / 64); i++) {
		context->caps[i] = 0;
	}
}

/** Set the user ID in a security context.
 * @param context	Context to set in.
 * @param uid		User ID to set. */
static inline void security_context_set_uid(security_context_t *context, user_id_t uid) {
	context->uid = uid;
}

/** Check if a security context is a member of a group.
 * @param context	Context to check.
 * @param gid		Group ID to check for.
 * @return		Whether the context has the specified group. */
static inline bool security_context_has_group(security_context_t *context, group_id_t gid) {
	size_t i;

	for(i = 0; i < SECURITY_MAX_GROUPS; i++) {
		if(context->groups[i] < 0) {
			break;
		} else if(context->groups[i] == gid) {
			return true;
		}
	}

	return false;
}

/** Add a group to a security context.
 * @note		Silently fails if group table full.
 * @note		Primary group is the first used entry in the structure.
 *			This group is used as the default owning group when one
 *			is not specified during object creation. The order of
 *			other groups is irrelevant.
 * @param context	Context to add to.
 * @param gid		Group ID to add. */
static inline void security_context_add_group(security_context_t *context, group_id_t gid) {
	size_t i;

	for(i = 0; i < SECURITY_MAX_GROUPS; i++) {
		if(context->groups[i] < 0) {
			context->groups[i] = gid;
			return;
		}
	}
}

/** Remove a group from a security context.
 * @param context	Context to remove from.
 * @param gid		Group ID to remove. */
static inline void security_context_remove_group(security_context_t *context, group_id_t gid) {
	size_t i;

	for(i = 0; i < SECURITY_MAX_GROUPS; i++) {
		if(context->groups[i] == gid) {
			context->groups[i] = -1;
			return;
		}
	}
}

/** Check if a security context has a capability.
 * @param context	Context to check in.
 * @param cap		Capability to check for.
 * @return		Whether context has the capability. */
static inline bool security_context_has_cap(const security_context_t *context, int cap) {
	return (context->caps[cap / 64] & (1 << (cap % 64)));
}

/** Set a capability in a security context.
 * @param context	Context to set in.
 * @param cap		Capability to set. */
static inline void security_context_set_cap(security_context_t *context, int cap) {
	context->caps[cap / 64] |= (1 << (cap % 64));
}

/** Remove a capability from a security context.
 * @param context	Context to remove from.
 * @param cap		Capability to remove. */
static inline void security_context_unset_cap(security_context_t *context, int cap) {
	context->caps[cap / 64] &= ~(1 << (cap % 64));
}

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SECURITY_H */
