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
 * @brief		POSIX get PID function.
 */

#include <kernel/process.h>
#include <unistd.h>

/** Get the current process ID.
 * @return		ID of calling process. */
pid_t getpid(void) {
	return kern_process_id(-1);
}

/** Get the parent process ID.
 * @return		ID of the parent process. */
pid_t getppid(void) {
	/* TODO. */
	return 0;
}
