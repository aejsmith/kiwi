/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Internal thread functions/definitions.
 */

#pragma once

#include <kernel/thread.h>

__KERNEL_EXTERN_C_BEGIN

#ifdef __KERNEL_PRIVATE

/** Actions for kern_thread_control(). */
#define THREAD_SET_TLS_ADDR     1   /**< Set TLS base address. */

extern status_t kern_thread_control(uint32_t action, const void *in, void *out);
extern void kern_thread_restore(void);

typedef void (*thread_dtor_t)(void);

extern status_t kern_thread_add_dtor(thread_dtor_t dtor);

#ifdef __LIBKERNEL

extern status_t _kern_thread_create(
    const char *name, thread_entry_t entry, void *arg,
    const thread_stack_t *stack, uint32_t flags, handle_t *_handle);
extern status_t _kern_thread_id(handle_t handle, thread_id_t *_id);
extern void _kern_thread_exit(int status) __kernel_noreturn;

#endif /* __LIBKERNEL */
#endif /* __KERNEL_PRIVATE */

__KERNEL_EXTERN_C_END
