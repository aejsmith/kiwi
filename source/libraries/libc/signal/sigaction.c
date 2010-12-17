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
 * @brief		Signal handling functions.
 */

#include <kernel/signal.h>
#include <kernel/status.h>

#include <errno.h>
#include <signal.h>

#include "../libc.h"

/** Examine or change the action of a signal.
 * @param num		Signal number to modify.
 * @param act		Pointer to new action for signal (can be NULL).
 * @param oldact	Pointer to location to store previous action in (can
 *			be NULL).
 * @return		0 on success, -1 on failure. */
int sigaction(int num, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
	status_t ret;

	ret = kern_signal_action(num, act, oldact);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	return 0;
}

/** Set the handler of a signal.
 * @param num		Signal number.
 * @param handler	Handler function.
 * @return		Previous handler, or SIG_ERR on failure. */
sighandler_t signal(int num, sighandler_t handler) {
	struct sigaction act;

	act.sa_handler = handler;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	if(sigaction(num, &act, &act) != 0) {
		return SIG_ERR;
	}

	return act.sa_handler;
}
