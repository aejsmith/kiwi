/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		POSIX signal send functions.
 */

#include <kernel/process.h>
#include <kernel/signal.h>
#include <kernel/status.h>

#include <errno.h>
#include <signal.h>

#include "libsystem.h"

/** Send a signal to a process.
 * @param pid		ID of process.
 * @param num		Signal number.
 * @return		0 on success, -1 on failure. */
int kill(pid_t pid, int num) {
	handle_t handle;
	status_t ret;

	if(pid < 1) {
		libsystem_stub("kill(pid < 0)", false);
		return -1;
	}

	ret = kern_process_open(pid, &handle);
	if(ret != STATUS_SUCCESS) {
		switch(ret) {
		case STATUS_ACCESS_DENIED:
			errno = EPERM;
			break;
		case STATUS_NOT_FOUND:
			errno = ESRCH;
			break;
		default:
			libsystem_status_to_errno(ret);
			break;
		}
		return -1;
	}

	ret = kern_signal_send(handle, num);
	kern_handle_close(handle);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Send a signal to the current process.
 * @param num		Signal number.
 * @return		0 on success, -1 on failure. */
int raise(int num) {
	status_t ret;

	ret = kern_signal_send(PROCESS_SELF, num);
	if(ret != STATUS_SUCCESS) {
		libsystem_status_to_errno(ret);
		return -1;
	}

	return 0;
}
