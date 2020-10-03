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
 * @brief               Process functions.
 */

#include <kernel/private/process.h>
#include <kernel/private/thread.h>

#include "libkernel.h"

/** Saved ID for the current process. */
process_id_t curr_process_id = -1;

/**
 * Clone the calling process.
 *
 * Creates a clone of the calling process. The new process will have a clone of
 * the original process' address space. Data in private mappings will be copied
 * when either the parent or the child writes to the pages. Non-private mappings
 * will be shared between the processes: any modifications made be either
 * process will be visible to the other. The new process will inherit all
 * handles from the parent, including non-inheritable ones (non-inheritable
 * handles are only closed when a new program is executed with
 * kern_process_exec() or kern_process_create()).
 *
 * Threads other than the calling thread are NOT cloned. The new process will
 * have a single thread which will resume execution after the call to
 * kern_process_clone().
 *
 * @param _handle       In the parent process, the location pointed to will be
 *                      set to a handle to the child process upon success. In
 *                      the child process, it will be set to INVALID_HANDLE.
 *
 * @return              Status code describing result of the operation.
 */
__sys_export status_t kern_process_clone(handle_t *_handle) {
    status_t ret;

    ret = _kern_process_clone(_handle);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* In the child, we must update the saved process and thread IDs. */
    if (*_handle == INVALID_HANDLE) {
        _kern_process_id(PROCESS_SELF, &curr_process_id);
        _kern_thread_id(THREAD_SELF, &curr_thread_id);
    }

    return STATUS_SUCCESS;
}

/** Get the ID of a process.
 * @param handle        Handle to process, or PROCESS_SELF for calling process.
 * @param _id           Where to store ID of process.
 * @return              Status code describing result of the operation. */
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
