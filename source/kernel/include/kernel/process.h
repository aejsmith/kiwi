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
 * @brief               Process management functions.
 */

#pragma once

#include <kernel/exception.h>
#include <kernel/exit.h>
#include <kernel/object.h>
#include <kernel/security.h>

__KERNEL_EXTERN_C_BEGIN

/** Extended attributes for process creation. */
typedef struct process_attrib {
    /**
     * Security token for new process.
     *
     * Token containing the security context for the new process. If set to
     * INVALID_HANDLE, or no attributes structure is given, the new process
     * will inherit the security context of the calling process.
     */
    handle_t token;

    /**
     * Root port for the new process.
     *
     * Handle to root port for the new process. If set to INVALID_HANDLE, or no
     * attributes structure is given, the new process will inherit the calling
     * process' root port.
     */
    handle_t root_port;

    /**
     * Handle map.
     *
     * Array containing a mapping of handles to duplicate into the new process
     * from the calling process. The first ID of each entry specifies the handle
     * in the caller, and the second specifies the ID to give it in the child.
     * Handles specified by this array are duplicated regardless of the
     * inheritable flag on the handle. Handles to objects of types which are
     * non-transferrable cannot be duplicated and specifying one in this array
     * will result in an error. If the count field is less than or equal to 0,
     * this field can be NULL.
     */
    handle_t (*map)[2];

    /**
     * Size of handle map.
     *
     * Number of entries in the handle map. If 0, no handles will be duplicated
     * to the child process. If negative, or no attributes structure is given,
     * handles will be duplicated into the new process according to the
     * inheritable flag on each handle table entry.
     */
    ssize_t map_count;
} process_attrib_t;

/** Handle value used to refer to the current process. */
#define PROCESS_SELF            INVALID_HANDLE

/** Process object events. */
enum {
    PROCESS_EVENT_DEATH         = 1,        /**< Wait for process death. */
};

/** Process priority classes. */
enum {
    PRIORITY_CLASS_LOW          = 0,        /**< Low priority. */
    PRIORITY_CLASS_NORMAL       = 1,        /**< Normal priority. */
    PRIORITY_CLASS_HIGH         = 2,        /**< High priority. */
};

/** Process creation flags. */
enum {
    PROCESS_CREATE_CRITICAL     = (1<<0),   /**< Process is a critical system process. */
};

extern status_t kern_process_create(
    const char *path, const char *const args[], const char *const env[],
    uint32_t flags, const process_attrib_t *attrib, handle_t *_handle);
extern status_t kern_process_exec(
    const char *path, const char *const args[], const char *const env[],
    uint32_t flags, const process_attrib_t *attrib);
extern status_t kern_process_clone(handle_t *_handle);

extern status_t kern_process_open(process_id_t id, handle_t *_handle);
extern status_t kern_process_id(handle_t handle, process_id_t *_id);
extern status_t kern_process_access(handle_t handle);
extern status_t kern_process_security(handle_t handle, security_context_t *ctx);
extern status_t kern_process_port(handle_t handle, int32_t id, handle_t *_handle);
extern status_t kern_process_status(handle_t handle, int *_status, int *_reason);
extern status_t kern_process_kill(handle_t handle);

extern status_t kern_process_token(handle_t *_handle);
extern status_t kern_process_set_token(handle_t handle);
extern status_t kern_process_set_exception_handler(unsigned code, exception_handler_t handler);

extern void kern_process_exit(int status) __kernel_noreturn;

__KERNEL_EXTERN_C_END
