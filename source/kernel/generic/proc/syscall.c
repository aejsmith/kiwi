/* Kiwi system call dispatcher
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
 * @brief		System call dispatcher.
 */

#include <arch/syscall.h>

#include <console/kprintf.h>

#include <proc/process.h>
#include <proc/subsystem.h>
#include <proc/syscall.h>

#include <fatal.h>

/** System call dispatcher.
 *
 * Handles a system call from a userspace process. It simply forwards the call
 * to the function defined by the process' subsystem.
 *
 * @param frame		System call frame structure.
 *
 * @return		Return value of the system call.
 */
unative_t syscall_handler(syscall_frame_t *frame) {
	subsystem_t *subsystem = curr_proc->subsystem;

	/* Shouldn't receive system calls from processes with no subsystem. */
	if(!subsystem) {
		fatal("Received system call from process with no subsystem");
	} else if(!subsystem->syscalls) {
		fatal("Subsystem %s has no system calls", subsystem->name);
	}

	/* If the system call doesn't exist, raise an exception in the
	 * subsystem. */
	if(frame->id >= subsystem->syscall_count || !subsystem->syscalls[frame->id]) {
		/* TODO. */
		fatal("Meh, invalid syscall");
	}

	return subsystem->syscalls[frame->id](frame->p1, frame->p2, frame->p3,
	                                      frame->p4, frame->p5);
}
