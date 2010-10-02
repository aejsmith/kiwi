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
 * @brief		POSIX process wait functions.
 *
 * @todo		If a new process is created while a wait()/waitpid() is
 *			in progress, it won't be added to the wait. What is
 *			needed to fix this is to wait on the child process list
 *			lock as well, and if object_wait_multiple() signals that
 *			the lock has been released, we should rebuild the wait
 *			array and wait again.
 */

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/semaphore.h>
#include <kernel/status.h>

#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>

#include "../unistd/unistd_priv.h"

/** Wait for a child process to stop or terminate.
 * @param statusp	Where to store process exit status.
 * @return		ID of process that terminated, or -1 on failure. */
pid_t wait(int *statusp) {
	return waitpid(-1, statusp, 0);
}

/** Wait for a child process to stop or terminate.
 * @param pid		If greater than 0, a specific PID to wait on (must be a
 *			child of the process). If 0, the function waits for any
 *			children with the same PGID as the process. If -1, the
 *			function waits for any children.
 * @param statusp	Where to store process exit status.
 * @param flags		Flags modifying behaviour.
 * @return		ID of process that terminated, or -1 on failure. */
pid_t waitpid(pid_t pid, int *statusp, int flags) {
	object_event_t *events = NULL, *tmp;
	posix_process_t *proc;
	size_t count = 0, i;
	status_t ret;
	int status;

	if(pid == 0) {
		errno = ENOSYS;
		return -1;
	}

	semaphore_down(child_processes_lock, -1);

	/* Build an array of handles to wait for. */
	LIST_FOREACH(&child_processes, iter) {
		proc = list_entry(iter, posix_process_t, header);
		if(pid == -1 || proc->pid == pid) {
			tmp = realloc(events, sizeof(*events) * (count + 1));
			if(!tmp) {
				goto fail;
			}
			events = tmp;
			events[count].handle = proc->handle;
			events[count].event = PROCESS_EVENT_DEATH;
			events[count++].signalled = false;
		}
	}

	/* Check if we have anything to wait for. */
	if(!count) {
		errno = ECHILD;
		goto fail;
	}

	semaphore_up(child_processes_lock, 1);

	/* Wait for any of them to exit. */
	ret = object_wait(events, count, (flags & WNOHANG) ? 0 : -1);
	if(ret != STATUS_SUCCESS) {
		if(events) { free(events); }
		if(ret == STATUS_WOULD_BLOCK) {
			return 0;
		}
		libc_status_to_errno(ret);
		return -1;
	}

	/* Only take the first exited process. */
	for(i = 0; i < count; i++) {
		if(!events[i].signalled) {
			continue;
		}

		semaphore_down(child_processes_lock, -1);
		LIST_FOREACH(&child_processes, iter) {
			proc = list_entry(iter, posix_process_t, header);
			if(proc->handle == events[i].handle) {
				/* Get the exit status. TODO: signal/stopped. */
				process_status(proc->handle, &status);
				*statusp = (status << 8) | __WEXITED;
				ret = proc->pid;

				/* Clean up the process. */
				handle_close(proc->handle);
				list_remove(&proc->header);
				free(proc);
				goto out;
			}
		}
	}
fail:
	ret = -1;
out:
	if(events) { free(events); }
	semaphore_up(child_processes_lock, 1);
	return ret;
}
