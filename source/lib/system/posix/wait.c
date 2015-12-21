/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief               POSIX process wait functions.
 *
 * TODO:
 *  - If a new process is created while a wait()/waitpid() is in progress, it
 *    won't be added to the wait. Perhaps add a kernel event object that we wait
 *    on as well, signal that when a child is added.
 */

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <sys/wait.h>

#include <errno.h>
#include <stdlib.h>

#include "posix/posix.h"

/** Wait for a child process to stop or terminate.
 * @param _status       Where to store process exit status.
 * @return              ID of process that terminated, or -1 on failure. */
pid_t wait(int *_status) {
    return waitpid(-1, _status, 0);
}

/** Convert a process exit status/reason to a POSIX status. */
static inline int convert_exit_status(int status, int reason) {
    switch (reason) {
    case EXIT_REASON_NORMAL:
        return (status << 8) | __WEXITED;
    //case EXIT_REASON_SIGNAL:
    //  return (status << 8) | __WSIGNALED;
    default:
        libsystem_fatal("unhandled exit reason %s", reason);
    }
}

/** Wait for a child process to stop or terminate.
 * @param pid           If greater than 0, a specific PID to wait on (must be a
 *                      child of the process). If 0, the function waits for any
 *                      children with the same PGID as the process. If -1, the
 *                      function waits for any children.
 * @param _status       Where to store process exit status.
 * @param flags         Flags modifying behaviour.
 * @return              ID of process that terminated, or -1 on failure. */
pid_t waitpid(pid_t pid, int *_status, int flags) {
    object_event_t *events = NULL, *tmp;
    posix_process_t *process;
    size_t count = 0, i;
    int status, reason;
    status_t ret;

    if (pid == 0) {
        // TODO: Process groups.
        errno = ENOSYS;
        return -1;
    }

    kern_mutex_lock(&child_processes_lock, -1);

    /* Build an array of handles to wait for. */
    core_list_foreach(&child_processes, iter) {
        process = core_list_entry(iter, posix_process_t, header);
        if (pid == -1 || process->pid == pid) {
            tmp = realloc(events, sizeof(*events) * (count + 1));
            if (!tmp)
                goto fail;

            events = tmp;
            events[count].handle = process->handle;
            events[count].event = PROCESS_EVENT_DEATH;
            events[count].flags = 0;
            count++;
        }
    }

    /* Check if we have anything to wait for. */
    if (!count) {
        errno = ECHILD;
        goto fail;
    }

    kern_mutex_unlock(&child_processes_lock);

    /* Wait for any of them to exit. */
    ret = kern_object_wait(events, count, 0, (flags & WNOHANG) ? 0 : -1);
    if (ret != STATUS_SUCCESS) {
        free(events);

        if (ret == STATUS_WOULD_BLOCK)
            return 0;

        libsystem_status_to_errno(ret);
        return -1;
    }

    kern_mutex_lock(&child_processes_lock, -1);

    /* Only take the first exited process. */
    for (i = 0; i < count; i++) {
        if (!(events[i].flags & OBJECT_EVENT_SIGNALLED))
            continue;

        core_list_foreach(&child_processes, iter) {
            process = core_list_entry(iter, posix_process_t, header);

            if (process->handle == events[i].handle) {
                /* Get the exit status. */
                if (_status) {
                    kern_process_status(process->handle, &status, &reason);
                    *_status = convert_exit_status(status, reason);
                }

                ret = process->pid;

                /* Clean up the process. */
                kern_handle_close(process->handle);
                core_list_remove(&process->header);
                free(process);
                goto out;
            }
        }
    }

fail:
    ret = -1;

out:
    free(events);
    kern_mutex_unlock(&child_processes_lock);
    return ret;
}
