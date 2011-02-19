/*
 * Copyright (C) 2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
#include <kernel/status.h>

#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>

#include "posix_priv.h"

/** Wait for a child process to stop or terminate.
 * @param statusp	Where to store process exit status.
 * @return		ID of process that terminated, or -1 on failure. */
pid_t wait(int *statusp) {
	return waitpid(-1, statusp, 0);
}

/** Convert a process exit status/reason to a POSIX status. */
static inline int convert_exit_status(int status, int reason) {
	switch(reason) {
	case EXIT_REASON_NORMAL:
		return (status << 8) | __WEXITED;
	case EXIT_REASON_SIGNAL:
		return (status << 8) | __WSIGNALED;
	default:
		libc_fatal("unhandled exit reason %s", reason);
	}
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
	int status, reason;
	status_t ret;

	if(pid == 0) {
		errno = ENOSYS;
		return -1;
	}

	libc_mutex_lock(&child_processes_lock, -1);

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

	libc_mutex_unlock(&child_processes_lock);

	/* Wait for any of them to exit. */
	ret = kern_object_wait(events, count, (flags & WNOHANG) ? 0 : -1);
	if(ret != STATUS_SUCCESS) {
		if(events) { free(events); }
		if(ret == STATUS_WOULD_BLOCK) {
			return 0;
		}
		libc_status_to_errno(ret);
		return -1;
	}

	libc_mutex_lock(&child_processes_lock, -1);

	/* Only take the first exited process. */
	for(i = 0; i < count; i++) {
		if(!events[i].signalled) {
			continue;
		}

		LIST_FOREACH(&child_processes, iter) {
			proc = list_entry(iter, posix_process_t, header);
			if(proc->handle == events[i].handle) {
				/* Get the exit status. */
				if(statusp) {
					kern_process_status(proc->handle, &status, &reason);
					*statusp = convert_exit_status(status, reason);
				}
				ret = proc->pid;

				/* Clean up the process. */
				kern_handle_close(proc->handle);
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
	libc_mutex_unlock(&child_processes_lock);
	return ret;
}
