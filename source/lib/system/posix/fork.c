/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               POSIX process creation function.
 */

#include <kernel/object.h>
#include <kernel/process.h>
#include <kernel/status.h>

#include <stdlib.h>

#include "posix/posix.h"

/** Structure containing a fork handler. */
typedef struct fork_handler {
    core_list_t header;         /**< List link. */
    void (*func)(void);         /**< Function to call. */
} fork_handler_t;

/** List of fork handlers. */
static CORE_LIST_DEFINE(fork_handlers);

/** List of child processes created via fork(). */
CORE_LIST_DEFINE(child_processes);

/** Lock for child process list. */
CORE_MUTEX_DEFINE(child_processes_lock);

/**
 * Create a clone of the calling process.
 *
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
    posix_process_t *process;
    fork_handler_t *handler;
    status_t ret;

    /* Allocate a child process structure. Do this before creating the process
     * so we don't fail after creating the child. */
    process = malloc(sizeof(*process));
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
            process = core_list_entry(iter, posix_process_t, header);

            /* Handles are all invalid as they should not be marked as
             * inheritable, but try to close them anyway just in case the user
             * is doing something daft. */
            kern_handle_close(process->handle);
            core_list_remove(&process->header);
            free(process);
        }

        /* Run post-fork handlers. */
        core_list_foreach(&fork_handlers, iter) {
            handler = core_list_entry(iter, fork_handler_t, header);
            handler->func();
        }

        return 0;
    } else {
        core_list_init(&process->header);
        process->pid = kern_process_id(process->handle);
        if (process->pid < 1)
            libsystem_fatal("could not get ID of child");

        /* Add it to the child list. */
        core_mutex_lock(&child_processes_lock, -1);
        core_list_append(&child_processes, &process->header);
        core_mutex_unlock(&child_processes_lock);

        return process->pid;
    }
}

/** Register a function to be called after a fork in the child.
 * @param func          Function to call. */
void register_fork_handler(void (*func)(void)) {
    fork_handler_t *handler;

    handler = malloc(sizeof(*handler));
    if (!handler)
        libsystem_fatal("failed to register fork handler");

    handler->func = func;
    core_list_init(&handler->header);
    core_list_append(&fork_handlers, &handler->header);
}
