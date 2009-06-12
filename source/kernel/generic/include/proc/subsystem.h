/* Kiwi process subsystem structure
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
 * @brief		Process subsystem structure.
 */

#ifndef __PROC_SUBSYSTEM_H
#define __PROC_SUBSYSTEM_H

#include <proc/process.h>

#include <types.h>

/** Structure defining an application subsystem. */
typedef struct subsystem {
	const char *name;		/**< Name of subsystem (for debugging purposes). */

	/** Initialize a process using this subsystem.
	 * @param process	Process to initialize.
	 * @return		0 on success, negative error code on failure. */
	int (*process_init)(process_t *process);

	/** Handle an exception caused in a thread on this subsystem.
	 * @note		If this returns, it means execution should
	 *			continue. If it is desired to kill the process
	 *			or thread, this handler should do so.
	 * @param thread	Thread that caused the exception. */
	int (*thread_exception)(thread_t *thread);
} subsystem_t;

#endif /* __PROC_SUBSYSTEM_H */
