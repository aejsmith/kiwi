/*
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Abort function.
 */

#include <signal.h>
#include <stdlib.h>

/** Abort program execution. */
void abort(void) {
	sigset_t set;

	/* First time we raise we just ensure that the signal is unblocked. */
	sigemptyset(&set);
	sigaddset(&set, SIGABRT);
	sigprocmask(SIG_UNBLOCK, &set, NULL);
	raise(SIGABRT);

	/* If we're still alive, we reset the signal to the default action and
	 * then raise again. */
	signal(SIGABRT, SIG_DFL);
	raise(SIGABRT);
	_Exit(255);
}
