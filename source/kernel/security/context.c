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
 * @brief		Security context functions.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <security/context.h>

#include <status.h>

/** Comparison function. */
static int compare_group(const void *a, const void *b) {
	group_id_t ga = *(const group_id_t *)a;
	group_id_t gb = *(const group_id_t *)b;
	return ga - gb;
}

/** Compare identity of two security contexts.
 * @fixme		This is pretty inefficient. Can't be bothered to think
 *			of a better way to do it at the moment.
 * @param a		First context.
 * @param b		Second context.
 * @return		True if the same, false if not. */
static inline bool compare_identity(const security_context_t *a, const security_context_t *b) {
	group_id_t *ga, *gb;
	bool ret;

	if(a->uid != b->uid) {
		return false;
	}

	ga = kmalloc(sizeof(a->groups), MM_SLEEP);
	memcpy(ga, a->groups, sizeof(a->groups));
	qsort(ga, ARRAYSZ(a->groups), sizeof(a->groups[0]), compare_group);
	gb = kmalloc(sizeof(b->groups), MM_SLEEP);
	memcpy(gb, b->groups, sizeof(b->groups));
	qsort(gb, ARRAYSZ(b->groups), sizeof(b->groups[0]), compare_group);
	ret = (memcmp(ga, gb, sizeof(a->groups)) == 0);
	kfree(ga);
	kfree(gb);
	return ret;
}

/** Validate a security context.
 *
 * Validates a security context to check that it does not have any capabilities
 * that the security context of the process trying to set it does not have,
 * and that the identity is not being changed if the setter is not allowed to
 * do so.
 *
 * @note		This does not check whether the process making the
 *			change is allowed to change the context.
 *
 * @param setter	Security context of process making the change.
 * @param prev		Previous security context.
 * @param context	New security context.
 *
 * @return		STATUS_SUCCESS if change is allowed, other status code
 *			if not.
 */
status_t security_context_validate(const security_context_t *setter,
                                   const security_context_t *prev,
                                   const security_context_t *context) {
	size_t i;

	/* Must have at least one group. */
	for(i = 0; i < ARRAYSZ(context->groups); i++) {
		if(context->groups[i] >= 0) {
			break;
		}
	}
	if(i == ARRAYSZ(context->groups)) {
		return STATUS_INVALID_ARG;
	}

	/* If the identity is different, check if the setter can change it. */
	if(!compare_identity(prev, context)) {
		if(!security_context_has_cap(setter, CAP_CHANGE_IDENTITY)) {
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

/** Initialise a security context with full priveleges.
 * @note		Must ONLY be used to create the kernel process.
 * @param context	Context to initialise. */
void security_context_full_init(security_context_t *context) {
	security_context_init(context);
	security_context_add_group(context, 0);
	security_context_set_cap(context, CAP_PROCESS_SECURITY);
	security_context_set_cap(context, CAP_CREATE_SESSION);
	security_context_set_cap(context, CAP_CHANGE_IDENTITY);
	security_context_set_cap(context, CAP_MODULE);
}
