/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Security context functions.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <security/context.h>

#include <status.h>

/** Initial security context. */
security_context_t init_security_context __init_data;

/** Comparison function. */
static int compare_group(const void *a, const void *b) {
	group_id_t ga = *(const group_id_t *)a;
	group_id_t gb = *(const group_id_t *)b;

	/* This forces negative entries to be last in the array. */
	if((ga < 0 && gb < 0) || (ga >= 0 && gb >= 0)) {
		return ga - gb;
	} else if(ga < 0) {
		return 1;
	} else {
		return -1;
	}
}

/**
 * Canonicalise a security context.
 *
 * Converts a security context to canonical form. A security context is
 * considered to be canonical if all group IDs after the primary group are in
 * ascending order with no unused entries between (all unused entries are at
 * the end), and there are no duplicate IDs. This is done to make comparison of
 * contexts and group checks easier.
 *
 * @param context	Context to canonicalise.
 */
void security_context_canonicalise(security_context_t *context) {
	size_t i;

	/* Move the first non-negative group to the first entry. This is the
	 * primary group. */
	for(i = 0; i < ARRAYSZ(context->groups); i++) {
		if(context->groups[i] >= 0) {
			if(i != 0) {
				context->groups[0] = context->groups[i];
				context->groups[i] = -1;
			}
			break;
		}
	}

	/* Sort the remaining groups into the required order. */
	qsort(&context->groups[1], ARRAYSZ(context->groups) - 1, sizeof(context->groups[0]), compare_group);
}

/** Compare identity of two security contexts.
 * @note		This only works if both contexts are in canonical form.
 * @param a		First context.
 * @param b		Second context.
 * @return		True if the same, false if not. */
static inline bool compare_identity(const security_context_t *a, const security_context_t *b) {
	if(a->uid != b->uid) {
		return false;
	}

	return (memcmp(a->groups, b->groups, sizeof(a->groups)) == 0);
}

/**
 * Validate a security context.
 *
 * Validates a security context to check that it does not have any capabilities
 * that the security context of the process trying to set it does not have,
 * and that the identity is not being changed if the setter is not allowed to
 * do so.
 *
 * @note		This does not check whether the process making the
 *			change is allowed to change the context.
 *
 * @param setter	Security context of process making the change. Must be
 *			in canonical form.
 * @param prev		Previous security context. Must be in canonical form.
 * @param context	New security context. This will be canonicalised using
 *			security_context_canonicalise(): there is no need to
 *			call that manually before calling this.
 *
 * @return		STATUS_SUCCESS if change is allowed, other status code
 *			if not.
 */
status_t security_context_validate(const security_context_t *setter,
                                   const security_context_t *prev,
                                   security_context_t *context) {
	size_t i;

	/* Convert the new context into canonical form. */
	security_context_canonicalise(context);

	/* Must have at least one group. */
	if(context->groups[0] < 0) {
		return STATUS_INVALID_ARG;
	}

	/* Ensure that the identity is the same if unable to change it. */
	if(!security_context_has_cap(setter, CAP_CHANGE_IDENTITY)) {
		if(!compare_identity(prev, context)) {
			return STATUS_PERM_DENIED;
		}
	}

	/* Compare capabilities: cannot set capabilities that the setter does
	 * not have.*/
	for(i = 0; i < ARRAYSZ(context->caps); i++) {
		if(context->caps[i] & ~(setter->caps[i])) {
			return STATUS_PERM_DENIED;
		}
	}

	return STATUS_SUCCESS;
}

/**
 * Obtain the security context for a process.
 *
 * Obtains the security context for a process. This function must always be
 * used to get the security context rather than accessing the process structure
 * directly. When you are finished with the context you must call
 * security_context_release() to unlock the context.
 *
 * @param process	Process to get context of, or NULL to get the context
 *			of the current process.
 *
 * @return		Security context for the process.
 */
security_context_t *security_context_get(process_t *process) {
	if(!process) {
		if(unlikely(!curr_thread)) {
			return &init_security_context;
		}
		process = curr_proc;
	}

	/* Take the security lock of the process. The purpose of this lock is
	 * to ensure that the security context will not be changed while access
	 * checks are performed using the context. It is unlocked by
	 * security_context_release(). The mutex is created with the
	 * MUTEX_RECURSIVE flag, meaning multiple calls to this function for
	 * one process are OK. */
	mutex_lock(&process->security_lock);
	return &process->security;
}

/** Drop a process' security context lock.
 * @return		Process to drop lock on, NULL for current process. */
void security_context_release(process_t *process) {
	if(!process) {
		if(unlikely(!curr_thread)) {
			return;
		}
		process = curr_proc;
	}

	mutex_unlock(&process->security_lock);
}

/** Get the user ID of the current thread.
 * @return		User ID of the current thread. */
user_id_t security_current_uid(void) {
	if(likely(curr_thread)) {
		return curr_proc->security.uid;
	} else {
		return init_security_context.uid;
	}
}

/** Get the primary group ID of the current thread.
 * @return		Primary group ID of the current thread. */
group_id_t security_current_gid(void) {
	if(likely(curr_thread)) {
		return curr_proc->security.groups[0];
	} else {
		return init_security_context.groups[0];
	}
}

/** Initialize the security system. */
__init_text void security_init(void) {
	security_context_init(&init_security_context);
	security_context_set_uid(&init_security_context, 0);
	security_context_add_group(&init_security_context, 0);

	/* Grant all capabilities to the initial security context, which is
	 * used for the kernel process and for the first userspace process.
	 * They will be dropped as required. */
	security_context_set_cap(&init_security_context, CAP_SECURITY_AUTHORITY);
	security_context_set_cap(&init_security_context, CAP_CREATE_SESSION);
	security_context_set_cap(&init_security_context, CAP_CHANGE_IDENTITY);
	security_context_set_cap(&init_security_context, CAP_MODULE);
	security_context_set_cap(&init_security_context, CAP_FS_ADMIN);
	security_context_set_cap(&init_security_context, CAP_FS_SETROOT);
	security_context_set_cap(&init_security_context, CAP_FS_MOUNT);
	security_context_set_cap(&init_security_context, CAP_CHANGE_OWNER);
	security_context_set_cap(&init_security_context, CAP_FATAL);
	security_context_set_cap(&init_security_context, CAP_SHUTDOWN);
}
