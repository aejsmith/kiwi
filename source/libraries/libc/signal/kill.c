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
 * @brief		POSIX signal send functions.
 */

#include <signal.h>
#include "../libc.h"

/** Send a signal to a process.
 * @param pid		ID of process.
 * @param num		Signal number.
 * @return		0 on success, -1 on failure. */
int kill(pid_t pid, int num) {
	libc_stub("kill", false);
	return -1;
}

/** Send a signal to the current process.
 * @param num		Signal number.
 * @return		0 on success, -1 on failure. */
int raise(int num) {
	libc_stub("raise", true);
	return -1;
}