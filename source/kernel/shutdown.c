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

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <io/fs.h>

#include <kernel/system.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <security/cap.h>

#include <console.h>
#include <kernel.h>
#include <status.h>

/** Whether a system shutdown is in progress. */
bool shutdown_in_progress = false;

/** IPI handler to halt a CPU. */
static status_t shutdown_ipi_handler(void *message, unative_t a1, unative_t a2, unative_t a3, unative_t a4) {
	ipi_acknowledge(message, STATUS_SUCCESS);
	cpu_halt();
}

/** System shutdown thread.
 * @param _action	Action to perform once the system has been shut down.
 * @param arg2		Unused. */
static void shutdown_thread_entry(void *_action, void *arg2) {
	int action = (int)((ptr_t)_action);

	kprintf(LOG_NORMAL, "system: terminating all processes...\n");
	process_shutdown();
	kprintf(LOG_NORMAL, "system: unmounting filesystems...\n");
	fs_shutdown();
	kprintf(LOG_NORMAL, "system: shutting down secondary CPUs...\n");
	ipi_broadcast(shutdown_ipi_handler, 0, 0, 0, 0, IPI_SEND_SYNC);

	switch(action) {
	case SHUTDOWN_REBOOT:
		kprintf(LOG_NORMAL, "system: rebooting...\n");
		platform_reboot();
		break;
	case SHUTDOWN_POWEROFF:
		kprintf(LOG_NORMAL, "system: powering off...\n");
		platform_poweroff();
		break;
	}

	kprintf(LOG_NORMAL, "system: halted.\n");
	cpu_halt();
}

/** Shut down the system.
 *
 * Terminates all running processes, flushes and unmounts all filesystems, and
 * then performs the specified action. The CAP_SHUTDOWN capability is required
 * to use this function.
 *
 * @param action	Action to perform once the system has been shut down.
 *
 * @return		Status code describing result of the operation. Will
 *			always succeed unless the calling process does not have
 *			the CAP_SHUTDOWN capability.
 */
status_t sys_system_shutdown(int action) {
	WAITQ_DECLARE(shutdown_wait);
	thread_t *thread;
	status_t ret;

	if(!cap_check(NULL, CAP_SHUTDOWN)) {
		return STATUS_PERM_DENIED;
	}

	if(!shutdown_in_progress) {
		shutdown_in_progress = true;

		/* Perform the shutdown in a thread under the kernel process,
		 * as all other processes will be terminated. Don't use a DPC,
		 * as it's possible that parts of the shutdown process will use
		 * them, and if we're running in one, we'll block those DPCs
		 * from executing. */
		ret = thread_create("shutdown", NULL, THREAD_UNPREEMPTABLE, shutdown_thread_entry,
		                    (void *)((ptr_t)action), NULL, NULL, &thread);
		if(ret != STATUS_SUCCESS) {
			/* FIXME: This shouldn't be able to fail, must reserve
			 * a thread or something in case we've got too many
			 * threads running. */
			kprintf(LOG_WARN, "system: unable to create shutdown thread (%d)\n", ret);
			shutdown_in_progress = false;
			return ret;
		}

		sched_preempt_disable();
		thread_run(thread);
	}

	/* The process shutdown code will interrupt us when it wants to kill
	 * this thread. */
	waitq_sleep(&shutdown_wait, -1, SYNC_INTERRUPTIBLE);
	thread_exit();
}
