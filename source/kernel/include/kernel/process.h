/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

/**
 * Extended attributes for process creation. Use process_attrib_init() to
 * ensure that this is initialised to sane defaults.
 */
typedef struct process_attrib {
    /**
     * Token containing the security context for the new process. If set to
     * INVALID_HANDLE, or no attributes structure is given, the new process
     * will inherit the security context of the calling process.
     */
    handle_t token;

    /**
     * Handle to root port for the new process. If set to INVALID_HANDLE, or no
     * attributes structure is given, the new process will inherit the calling
     * process' root port.
     */
    handle_t root_port;

    /**
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
     * Number of entries in the handle map. If 0, no handles will be duplicated
     * to the child process. If negative, or no attributes structure is given,
     * handles will be duplicated into the new process according to the
     * inheritable flag on each handle table entry.
     */
    ssize_t map_count;
} process_attrib_t;

/**
 * Constructor for process_attrib_t with default values that will behave as
 * though no attrib structure was passed.
 *
 * @param attrib        Attributes to initialise.
 */
static inline void process_attrib_init(process_attrib_t *attrib) {
    attrib->token     = INVALID_HANDLE;
    attrib->root_port = INVALID_HANDLE;
    attrib->map       = NULL;
    attrib->map_count = -1;
}

/**
 * Process arguments. This is what is passed into a process at launch. It is
 * saved in libkernel and can be retrieved with kern_process_args().
 */
typedef struct process_args {
    char *path;                             /**< Path to program. */
    char **args;                            /**< Argument array. */
    char **env;                             /**< Environment variable array. */
    size_t arg_count;                       /**< Number of entries in argument array (excluding NULL). */
    size_t env_count;                       /**< Number of entries in environment array (excluding NULL). */
    void *load_base;                        /**< Load base of libkernel. */
} process_args_t;

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

extern const process_args_t *kern_process_args(void);

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
extern status_t kern_process_kill(handle_t handle, int status);

extern status_t kern_process_token(handle_t *_handle);
extern status_t kern_process_set_token(handle_t handle);
extern status_t kern_process_set_exception_handler(uint32_t code, exception_handler_t handler);

extern void kern_process_exit(int status) __kernel_noreturn;

__KERNEL_EXTERN_C_END
