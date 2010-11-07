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

#include <util/mutex.h>

#include <setjmp.h>
#include <stdlib.h>

#include "posix_priv.h"

/** List of child processes created via fork(). */
LIST_DECLARE(child_processes);
LIBC_MUTEX_DECLARE(child_processes_lock);

/** Fork entry point.
 * @param arg		Pointer to jump buffer. */
static void fork_entry(void *arg) {
	longjmp(arg, 1);
}

/** Parent part of fork().
 * @param proc		Allocated process structure.
 * @param state		State pointer.
 * @param stack		Stack allocation.
 * @return		Process ID of child or -1 on failure. */
static pid_t fork_parent(posix_process_t *proc, jmp_buf state, char *stack) {
	handle_t handle;
	status_t ret;

	/* Clone the process, starting it at our entry function which restores
	 * the saved process. FIXME: Stack direction. */
	ret = process_clone(fork_entry, state, &stack[0x1000], NULL, PROCESS_QUERY, &handle);
	vm_unmap(stack, 0x1000);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return ret;
	}

	list_init(&proc->header);
	proc->handle = handle;
	proc->pid = process_id(proc->handle);
	if(proc->pid < 1) {
		libc_fatal("could not get ID of child");
	}

	/* Add it to the child list so that wait*() knows about it. */
	libc_mutex_lock(&child_processes_lock, -1);
	list_append(&child_processes, &proc->header);
	libc_mutex_unlock(&child_processes_lock);

	/* Parent returns PID of new process. */
	return proc->pid;
}

/** Child part of fork().
 * @param proc		Allocated process structure.
 * @param stack		Stack allocation.
 * @return		Return value for fork(). */
static pid_t fork_child(posix_process_t *proc, char *stack) {
	/* We're now back on the original stack, the temporary stack is no
	 * longer needed. */
	vm_unmap(stack, 0x1000);

	/* Free the unneeded process structure. */
	free(proc);

	/* Empty the child processes list: anything in there is not our child,
	 * but a child of our parent. */
	libc_mutex_lock(&child_processes_lock, -1);
	LIST_FOREACH_SAFE(&child_processes, iter) {
		proc = list_entry(iter, posix_process_t, header);
		handle_close(proc->handle);
		list_remove(&proc->header);
		free(proc);
	}
	libc_mutex_unlock(&child_processes_lock);

	/* Child returns 0. */
	return 0;
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
	posix_process_t *proc;
	jmp_buf state;
	status_t ret;
	void *stack;

	/* Allocate a process structure for the child. We must do this before
	 * the child is started so that we don't discover we are unable to
	 * allocate the structure after the child is started. */
	proc = malloc(sizeof(*proc));
	if(!proc) {
		return -1;
	}

	/* Create a temporary stack. FIXME: Page size is arch-dependent. */
	ret = vm_map(NULL, 0x1000, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, -1, 0, &stack);
	if(ret != STATUS_SUCCESS) {
		libc_status_to_errno(ret);
		return -1;
	}

	/* Save our execution state. */
	if(setjmp(state) > 0) {
		return fork_child(proc, stack);
	} else {
		return fork_parent(proc, state, stack);
	}
}
