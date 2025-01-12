/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Internal process functions/definitions.
 */

#pragma once

#include <kernel/process.h>

__KERNEL_EXTERN_C_BEGIN

#ifdef __KERNEL_PRIVATE

/** Actions for kern_process_control(). */
#define PROCESS_LOADED          1   /**< Signal that process is loaded. */
#define PROCESS_SET_RESTORE     2   /**< Set the thread restore function. */

extern status_t kern_process_control(uint32_t action, const void *in, void *out);

typedef void (*process_clone_handler_t)(void);
extern status_t kern_process_add_clone_handler(process_clone_handler_t handler);

#ifdef __LIBKERNEL

extern status_t _kern_process_clone(handle_t *_handle);
extern status_t _kern_process_id(handle_t handle, process_id_t *_id);

#endif /* __LIBKERNEL */
#endif /* __KERNEL_PRIVATE */

__KERNEL_EXTERN_C_END
