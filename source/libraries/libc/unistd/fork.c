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
 * @brief		POSIX process creation function.
 */

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/semaphore.h>
#include <kernel/status.h>
#include <kernel/thread.h>
#include <kernel/vm.h>

#include <setjmp.h>
#include <stdlib.h>

#include "unistd_priv.h"

/** List of child processes created via fork(). */
LIST_DECLARE(child_processes);
handle_t child_processes_lock = 0;

/** Fork entry point.
 * @param arg		Pointer to jump buffer. */
static void fork_entry(void *arg) {
	longjmp(arg, 1);
}

/** Parent part of fork().
 * @param state		State pointer.
 * @param stack		Stack allocation.
 * @return		Process ID of child or -1 on failure. */
static pid_t fork_parent(jmp_buf state, char *stack) {
	posix_process_t *proc;
	handle_t handle;
	status_t ret;

	/* Clone the process, starting it at our entry function which restores
	 * the saved process. FIXME: Stack direction. */
	ret = process_clone(fork_entry, state, &stack[0x1000], &handle);
	vm_unmap(stack, 0x1000);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		free(proc);
		return ret;
	}

	/* Create a structure to store details of the child and add it to the
	 * child list. */
	proc = malloc(sizeof(*proc));
	list_init(&proc->header);
	proc->handle = handle;
	proc->pid = process_id(proc->handle);
	if(proc->pid < 1) {
		libc_fatal("could not get ID of child");
	}
	semaphore_down(child_processes_lock, -1);
	list_append(&child_processes, &proc->header);
	semaphore_up(child_processes_lock, 1);
	return proc->pid;
}

/** Create a clone of the calling process.
 *
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to the pages. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process will inherit all
 * file descriptors from the parent, including ones marked as FD_CLOEXEC. Only
 * the calling thread will be duplicated, however. Other threads will not be
 * duplicated into the new process.
 *
 * @return		0 to the child process, process ID of the child to the
 *			parent, or -1 on failure, with errno set appropriately.
 */
pid_t fork(void) {
	jmp_buf state;
	status_t ret;
	void *stack;

	/* Create a temporary stack. FIXME: Page size is arch-dependent. */
	ret = vm_map(NULL, 0x1000, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, -1, 0, &stack);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	/* Save our execution state. */
	if(setjmp(state) > 0) {
		/* We're in the child. Clean up and return. */
		vm_unmap(stack, 0x1000);
		return 0;
	}

	return fork_parent(state, stack);
}

/** Create the child process list lock. */
static void __attribute__((constructor)) fork_init(void) {
	status_t ret = semaphore_create("child_processes_lock", 1, &child_processes_lock);
	if(ret != STATUS_SUCCESS) {
		libc_fatal("could not create child list lock (%d)", ret);
	}
}
