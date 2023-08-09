/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               Process functions.
 */

#include <kernel/private/process.h>
#include <kernel/private/thread.h>

#include "libkernel.h"

/** Process arguments. */
process_args_t *process_args = NULL;

/** Saved ID for the current process. */
process_id_t curr_process_id = -1;

/** Process clone handler functions. */
#define CLONE_HANDLER_MAX 8
static process_clone_handler_t process_clone_handlers[CLONE_HANDLER_MAX] = {};

__sys_export status_t kern_process_clone(handle_t *_handle) {
    status_t ret = _kern_process_clone(_handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* In the child, we must update the saved process and thread IDs. */
    if (*_handle == INVALID_HANDLE) {
        _kern_process_id(PROCESS_SELF, &curr_process_id);
        _kern_thread_id(THREAD_SELF, &curr_thread_id);

        for (size_t i = 0; i < CLONE_HANDLER_MAX; i++) {
            if (process_clone_handlers[i])
                process_clone_handlers[i]();
        }
    }

    return STATUS_SUCCESS;
}

__sys_export status_t kern_process_id(handle_t handle, process_id_t *_id) {
    /* We save the current process ID to avoid having to perform a kernel call
     * just to get our own ID. */
    if (handle == PROCESS_SELF) {
        *_id = curr_process_id;
        return STATUS_SUCCESS;
    } else {
        return _kern_process_id(handle, _id);
    }
}

/**
 * Add a handler function to be called in the child process after it has been
 * cloned. If the function already exists in the list then it will not be added
 * again.
 *
 * @param handler       Clone handler function.
 *
 * @return              STATUS_SUCCESS on success.
 *                      STATUS_NO_MEMORY if there is no space in the handler
 *                      list.
 */
__sys_export status_t kern_process_add_clone_handler(process_clone_handler_t handler) {
    size_t first_free = CLONE_HANDLER_MAX;
    for (size_t i = 0; i < CLONE_HANDLER_MAX; i++) {
        if (process_clone_handlers[i] == handler) {
            return STATUS_SUCCESS;
        } else if (!process_clone_handlers[i] && first_free == CLONE_HANDLER_MAX) {
            first_free = i;
        }
    }

    if (first_free < CLONE_HANDLER_MAX) {
        process_clone_handlers[first_free] = handler;
        return STATUS_SUCCESS;
    } else {
        return STATUS_NO_MEMORY;
    }
}

/** Gets the arguments for the current process. */
__sys_export const process_args_t *kern_process_args(void) {
    return process_args;
}
