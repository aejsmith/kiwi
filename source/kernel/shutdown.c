/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief		System shutdown code.
 */

#include <io/fs.h>

#include <kernel/system.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <cpu.h>
#include <kernel.h>
#include <smp.h>
#include <status.h>

/** Whether a system shutdown is in progress. */
bool shutdown_in_progress = false;

/** SMP call handler to halt a CPU. */
static status_t shutdown_call_func(void *data) {
	smp_call_acknowledge(STATUS_SUCCESS);
	arch_cpu_halt();
}

/** System shutdown thread.
 * @param _action	Action to perform once the system has been shut down.
 * @param arg2		Unused. */
static void shutdown_thread_entry(void *_action, void *arg2) {
	int action = (int)((ptr_t)_action);

	preempt_disable();

	kprintf(LOG_NOTICE, "system: terminating all processes...\n");
	process_shutdown();
	kprintf(LOG_NOTICE, "system: unmounting filesystems...\n");
	fs_shutdown();

	/* Halt all remote CPUs. */
	smp_call_broadcast(shutdown_call_func, NULL, 0);

	switch(action) {
	case SHUTDOWN_REBOOT:
		kprintf(LOG_NOTICE, "system: rebooting...\n");
		platform_reboot();
		break;
	case SHUTDOWN_POWEROFF:
		kprintf(LOG_NOTICE, "system: powering off...\n");
		platform_poweroff();
		break;
	}

	kprintf(LOG_NOTICE, "system: halted.\n");
	arch_cpu_halt();
}

/** Shut down the system.
 * @param action	Action to perform once the system has been shut down. */
void system_shutdown(unsigned action) {
	status_t ret;

	if(!shutdown_in_progress) {
		shutdown_in_progress = true;
		preempt_disable();

		/* Perform the shutdown in a thread under the kernel process,
		 * as all other processes will be terminated. Don't use a DPC,
		 * as it's possible that parts of the shutdown process will use
		 * them, and if we're running in one, we'll block those DPCs
		 * from executing. */
		ret = thread_create("shutdown", NULL, 0, shutdown_thread_entry,
			(void *)((ptr_t)action), NULL, NULL);
		if(ret != STATUS_SUCCESS) {
			/* FIXME: This shouldn't be able to fail, must reserve
			 * a thread or something in case we've got too many
			 * threads running. */
			fatal("Unable to create shutdown thread (%d)", ret);
		}
	}

	if(curr_proc != kernel_proc) {
		/* The process shutdown code will interrupt us when it wants to
		 * kill this thread. */
		thread_sleep(NULL, -1, "system_shutdown", SLEEP_INTERRUPTIBLE);
		thread_exit();
	}
}

/**
 * Shut down the system.
 *
 * Terminates all running processes, flushes and unmounts all filesystems, and
 * then performs the specified action.
 *
 * @param action	Action to perform once the system has been shut down.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_system_shutdown(unsigned action) {
	system_shutdown(action);
	fatal("Shouldn't get here");
}
