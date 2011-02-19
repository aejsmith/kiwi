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

	ret = kern_process_control(-1, PROCESS_GET_SECTX, NULL, &context);
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

	ret = kern_process_control(-1, PROCESS_GET_SECTX, NULL, &context);
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
