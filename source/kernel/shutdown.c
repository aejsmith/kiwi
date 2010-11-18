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
 * @brief		System shutdown code.
 */

#include <kernel/system.h>

#include <proc/thread.h>

#include <security/cap.h>

#include <console.h>
#include <status.h>

/** Whether a system shutdown is in progress. */
bool shutdown_in_progress = false;

/** Shut down the system.
 *
 * Terminates all running processes, flushes and unmounts all filesystems, and
 * then performs the specified action. The CAP_SHUTDOWN capability is required
 * to use this function.
 *
 * @param action	Action to perform once the system has been shutdown.
 *
 * @return		Status code describing result of the operation. Will
 *			always succeed unless the calling process does not have
 *			the CAP_SHUTDOWN capability.
 */
status_t sys_system_shutdown(int action) {
#if 0
	thread_t *thread;
	status_t ret;

	if(!cap_check(NULL, CAP_SHUTDOWN)) {
		return STATUS_PERM_DENIED;
	}

	/* Perform the shutdown in a thread under the kernel process, as all
	 * other processes will be terminated. */
	ret = thread_create("shutdown", NULL, THREAD_UNPREEMPTABLE, shutdown_thread_entry,
	                    (void *)((ptr_t)action), NULL, NULL, &thread);
	if(ret != STATUS_SUCCESS) {
		/* FIXME: This shouldn't be able to fail, must reserve a thread
		 * or something in case we've got too many threads running. */
		kprintf(LOG_WARN, "system: unable to create shutdown thread (%d)\n", ret);
		return ret;
	}
	thread_run(thread);

	process_exit(0);
#endif
	return STATUS_NOT_IMPLEMENTED;
}
