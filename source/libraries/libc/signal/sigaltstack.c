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
 * @brief		Alternate signal stack function.
 */

#include <kernel/signal.h>
#include <kernel/status.h>

#include <signal.h>

#include "../libc.h"

/** Get and set the alternate signal stack.
 *
 * Gets and sets the alternate signal stack for the current thread. This stack
 * is used to execute signal handlers with the SA_ONSTACK flag set. The
 * alternate stack is a per-thread attribute. If fork() is called, the new
 * process' initial thread inherits the alternate stack from the thread that
 * called fork().
 *
 * @param ss		Alternate stack to set (can be NULL).
 * @param oset		Where to store previous alternate stack (can be NULL).
 *
 * @return		0 on success, -1 on failure.
 */
int sigaltstack(const stack_t *restrict ss, stack_t *restrict oldss) {
	status_t ret;

	ret = kern_signal_altstack(ss, oldss);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}
