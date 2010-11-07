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
 * @brief		POSIX user/group functions.
 */

#include <kernel/process.h>
#include <kernel/security.h>
#include <kernel/status.h>

#include <errno.h>
#include <unistd.h>

#include "../libc.h"

/** Get the process' effective group ID.
 * @return		Effective group ID of the process. */
gid_t getegid(void) {
	security_context_t context;
	status_t ret;

	ret = process_security_context(-1, &context);
	if(ret != STATUS_SUCCESS) {
		libc_fatal("failed to obtain security context: %d", ret);
	}

	return context.groups[0];
}

/** Get the process' effective user ID.
 * @return		Effective user ID of the process. */
uid_t geteuid(void) {
	security_context_t context;
	status_t ret;

	ret = process_security_context(-1, &context);
	if(ret != STATUS_SUCCESS) {
		libc_fatal("failed to obtain security context: %d", ret);
	}

	return context.uid;
}

/** Get the process' group ID.
 * @return		Group ID of the process. */
gid_t getgid(void) {
	return getegid();
}

/** Get the process' user ID.
 * @return		User ID of the process. */
uid_t getuid(void) {
	return geteuid();
}

/** Set the group ID of the process.
 * @param gid		Group ID to set.
 * @return		0 on success, -1 on failure. */
int setgid(gid_t gid) {
	libc_stub("setgid", false);
	return -1;
}

/** Set the user ID of the process.
 * @param uid		User ID to set.
 * @return		0 on success, -1 on failure. */
int setuid(uid_t uid) {
	libc_stub("setuid", false);
	return -1;
}
