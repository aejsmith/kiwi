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

#include <errno.h>
#include <signal.h>

#include "../libc.h"

int sigaction(int num, const struct sigaction *restrict act, struct sigaction *restrict oldact) {
	//libc_stub("sigaction", false);
	if(oldact) {
		oldact->sa_handler = SIG_DFL;
		sigemptyset(&oldact->sa_mask);
		oldact->sa_flags = 0;
	}
	return 0;
}

/** Set the handler of a signal.
 * @param sig		Signal number.
 * @param handler	Handler function.
 * @return		Previous handler, or SIG_ERR on failure. */
void (*signal(int sig, void (*handler)(int)))(int) {
	//libc_stub("signal", false);
	return SIG_DFL;
}
