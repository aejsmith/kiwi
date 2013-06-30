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
