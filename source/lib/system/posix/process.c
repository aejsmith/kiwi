/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               POSIX process management functions.
 *
 * TODO:
 *  - If a new process is created while a wait()/waitpid() is in progress, it
 *    won't be added to the wait. Perhaps add a kernel event object that we wait
 *    on as well, signal that when a child is added.
 */

#include <core/ipc.h>

#include <kernel/object.h>
#include <kernel/private/process.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <services/posix_service.h>

#include <sys/wait.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include "posix/posix.h"

/** List of child processes created via fork(). */
CORE_LIST_DEFINE(child_processes);

/** Lock for child process list. */
CORE_MUTEX_DEFINE(child_processes_lock);

/**
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to them. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process will inherit all
 * file descriptors from the parent, including ones marked as FD_CLOEXEC. Only
 * the calling thread will be duplicated, however. Other threads will not be
 * duplicated into the new process.
 *
 * @return              0 in the child process, process ID of the child in the
 *                      parent, or -1 on failure, with errno set appropriately.
 */
pid_t fork(void) {
    status_t ret;

    posix_process_t *process = malloc(sizeof(*process));
    if (!process)
        return -1;

    ret = kern_process_clone(&process->handle);
    if (ret != STATUS_SUCCESS) {
        libsystem_status_to_errno(ret);
        free(process);
        return -1;
    }

    if (process->handle == INVALID_HANDLE) {
        /* This is the child. Free the unneeded process structure. */
        free(process);

        /* Empty the child processes list: anything in there is not our child,
         * but a child of our parent. */
        core_list_foreach_safe(&child_processes, iter) {
            posix_process_t *child = core_list_entry(iter, posix_process_t, header);

            /* Handles are all invalid as they should not be marked as
             * inheritable, but try to close them anyway just in case the user
             * is doing something daft. */
            kern_handle_close(child->handle);

            core_list_remove(&child->header);
            free(child);
        }

        return 0;
    } else {
        core_list_init(&process->header);

        ret = kern_process_id(process->handle, &process->pid);
        if (ret != STATUS_SUCCESS)
            libsystem_fatal("could not get ID of child");

        /* Add it to the child list. */
        core_mutex_lock(&child_processes_lock, -1);
        core_list_append(&child_processes, &process->header);
        core_mutex_unlock(&child_processes_lock);

        return process->pid;
    }
}

/** Registers a function to be called after a fork in the child.
 * @param func          Function to call. */
void posix_register_fork_handler(void (*func)(void)) {
    status_t ret __sys_unused = kern_process_add_clone_handler(func);
    libsystem_assert(ret == STATUS_SUCCESS);
}

/** Convert a process exit status/reason to a POSIX status. */
static inline int convert_exit_status(int status, int reason) {
    switch (reason) {
        case EXIT_REASON_NORMAL:
            return (status << 8) | __WEXITED;
        case EXIT_REASON_KILLED:
            if (((uint32_t)status >> 16) == __POSIX_KILLED_STATUS) {
                int32_t signal = status & 0xffff;
                if (signal >= 1 && signal < NSIG)
                    return (signal << 8) | __WSIGNALED;
            }

            /* Must have come from a non-POSIX use of kern_process_kill(), so
             * just say it was SIGKILL. */
            return (SIGKILL << 8) | __WSIGNALED;
        case EXIT_REASON_EXCEPTION:
            return (posix_signal_from_exception(status) << 8) | __WSIGNALED;
        default:
            libsystem_log(CORE_LOG_WARN, "unhandled exit reason %d", reason);
            return __WEXITED;
    }
}

/** Waits for a child process to stop or terminate.
 * @param pid           If greater than 0, a specific PID to wait on (must be a
 *                      child of the process). If 0, the function waits for any
 *                      children with the same PGID as the process. If -1, the
 *                      function waits for any children.
 * @param _status       Where to store process exit status.
 * @param flags         Flags modifying behaviour.
 * @return              ID of process that terminated, or -1 on failure. */
pid_t waitpid(pid_t pid, int *_status, int flags) {
    status_t ret;

    if (pid == 0) {
        // TODO: Process groups.
        errno = ENOSYS;
        return -1;
    }

    core_mutex_lock(&child_processes_lock, -1);

    /* Build an array of handles to wait for. */
    object_event_t *events = NULL;
    size_t count = 0;
    core_list_foreach(&child_processes, iter) {
        posix_process_t *process = core_list_entry(iter, posix_process_t, header);

        if (pid == -1 || process->pid == pid) {
            object_event_t *tmp = realloc(events, sizeof(*events) * (count + 1));
            if (!tmp)
                goto fail;

            events = tmp;

            events[count].handle = process->handle;
            events[count].event  = PROCESS_EVENT_DEATH;
            events[count].flags  = 0;

            count++;
        }
    }

    /* Check if we have anything to wait for. */
    if (!count) {
        errno = ECHILD;
        goto fail;
    }

    core_mutex_unlock(&child_processes_lock);

    /* Wait for any of them to exit. */
    ret = kern_object_wait(events, count, 0, (flags & WNOHANG) ? 0 : -1);
    if (ret != STATUS_SUCCESS) {
        free(events);

        if (ret == STATUS_WOULD_BLOCK)
            return 0;

        libsystem_status_to_errno(ret);
        return -1;
    }

    core_mutex_lock(&child_processes_lock, -1);

    /* Only take the first exited process. */
    for (size_t i = 0; i < count; i++) {
        if (!(events[i].flags & OBJECT_EVENT_SIGNALLED))
            continue;

        core_list_foreach(&child_processes, iter) {
            posix_process_t *process = core_list_entry(iter, posix_process_t, header);

            if (process->handle == events[i].handle) {
                /* Get the exit status. */
                if (_status) {
                    int status, reason;
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
    core_mutex_unlock(&child_processes_lock);
    return ret;
}

/** Waits for a child process to stop or terminate.
 * @param _status       Where to store process exit status.
 * @return              ID of process that terminated, or -1 on failure. */
pid_t wait(int *_status) {
    return waitpid(-1, _status, 0);
}

/** Gets the current process ID.
 * @return              ID of calling process. */
pid_t getpid(void) {
    process_id_t id;
    status_t ret = kern_process_id(PROCESS_SELF, &id);
    libsystem_assert(ret == STATUS_SUCCESS);
    return id;
}

/** Gets the parent process ID.
 * @return              ID of the parent process. */
pid_t getppid(void) {
    /* TODO. */
    return 0;
}

static bool getpgid_request(core_connection_t *conn, pid_t pid, pid_t *_pgid) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_GETPGID, sizeof(posix_request_getpgid_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_getpgid_t *request_data = core_message_data(request);
    request_data->pid = pid;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_getpgid_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    if (reply_err == 0)
        *_pgid = reply_data->pgid;

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/** Gets the process group ID of a process.
 * @param pid           PID to get for (or 0 for calling process).
 * @return              Process group ID, or -1 on failure. */
pid_t getpgid(pid_t pid) {
    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    pid_t pgid;
    bool success = getpgid_request(conn, pid, &pgid);

    posix_service_put();
    return (success) ? pgid : -1;
}

/** Gets the process group ID of the calling process.
 * @return              Process group ID, or -1 on failure. */
pid_t getpgrp(void) {
    return getpgid(0);
}

static bool setpgid_request(core_connection_t *conn, pid_t pid, pid_t pgid) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_SETPGID, sizeof(posix_request_setpgid_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_setpgid_t *request_data = core_message_data(request);
    request_data->pid  = pid;
    request_data->pgid = pgid;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_setpgid_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/** Sets the process group ID of a process.
 * @param pid           PID to set for (or 0 for calling process).
 * @param pgid          Process group ID (or 0 to match calling process ID).
 * @return              0 on success, -1 on failure. */
int setpgid(pid_t pid, pid_t pgid) {
    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    bool success = setpgid_request(conn, pid, pgid);

    posix_service_put();
    return (success) ? 0 : -1;
}

/** Sets the process group ID of the calling process to its process ID.
 * @return              0 on success, -1 on failure. */
int setpgrp(void) {
    return setpgid(0, 0);
}

static bool getsid_request(core_connection_t *conn, pid_t pid, pid_t *_sid) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_GETSID, sizeof(posix_request_getsid_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_getsid_t *request_data = core_message_data(request);
    request_data->pid = pid;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_getsid_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    if (reply_err == 0)
        *_sid = reply_data->sid;

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/** Gets the session ID of a process.
 * @param pid           PID to get for (or 0 for calling process).
 * @return              Session ID, or -1 on failure. */
pid_t getsid(pid_t pid) {
    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    pid_t sid;
    bool success = getsid_request(conn, pid, &sid);

    posix_service_put();
    return (success) ? sid : -1;
}

static bool setsid_request(core_connection_t *conn, pid_t *_sid) {
    core_message_t *request = core_message_create_request(POSIX_REQUEST_SETSID, 0, 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_setsid_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    if (reply_err == 0)
        *_sid = reply_data->sid;

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/** Sets the session and process group ID of the calling process.
 * @return              Session ID, or -1 on failure. */
pid_t setsid(void) {
    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    pid_t sid;
    bool success = setsid_request(conn, &sid);

    posix_service_put();
    return (success) ? sid : -1;
}

static bool posix_get_pgrp_session_request(core_connection_t *conn, pid_t pgid, pid_t *_sid) {
    core_message_t *request = core_message_create_request(
        POSIX_REQUEST_GET_PGRP_SESSION, sizeof(posix_request_get_pgrp_session_t), 0);
    if (!request) {
        errno = ENOMEM;
        return false;
    }

    posix_request_get_pgrp_session_t *request_data = core_message_data(request);
    request_data->pgid = pgid;

    core_message_t *reply;
    status_t ret = core_connection_request(conn, request, &reply);
    core_message_destroy(request);

    if (ret != STATUS_SUCCESS) {
        libsystem_log(CORE_LOG_ERROR, "failed to make POSIX request: %" PRId32, ret);
        libsystem_status_to_errno(ret);
        return false;
    }

    posix_reply_get_pgrp_session_t *reply_data = core_message_data(reply);
    int reply_err = reply_data->err;

    if (reply_err == 0)
        *_sid = reply_data->sid;

    core_message_destroy(reply);

    if (reply_err != 0) {
        errno = reply_err;
        return false;
    }

    return true;
}

/** Gets the ID of the session that a process group belongs to.
 * @param pgid          Process group ID.
 * @return              Session ID on success, -1 on failure:
 *                       - ESRCH if PGID does not refer to a valid process
 *                         group. */
pid_t posix_get_pgrp_session(pid_t pgid) {
    core_connection_t *conn = posix_service_get();
    if (!conn) {
        errno = EAGAIN;
        return -1;
    }

    pid_t sid;
    bool success = posix_get_pgrp_session_request(conn, pgid, &sid);

    posix_service_put();
    return (success) ? sid : -1;
}
