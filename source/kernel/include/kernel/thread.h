/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Thread management functions.
 */

#pragma once

#include <kernel/context.h>
#include <kernel/exception.h>
#include <kernel/limits.h>
#include <kernel/object.h>
#include <kernel/security.h>

__KERNEL_EXTERN_C_BEGIN

/** Thread stack information. */
typedef struct thread_stack {
    /**
     * Base of stack.
     *
     * Base address of the stack area for the process. The kernel deals with
     * setting the stack pointer within the specified area. When creating a new
     * thread, if the base is NULL, a stack will be allocated by the kernel,
     * and will be freed automatically when the thread terminates. If not NULL,
     * it is the responsibility of the program to free the stack after the
     * thread terminates.
     */
    void *base;

    /**
     * Size of the stack.
     *
     * If stack is not NULL, then this should be the non-zero size of the
     * provided stack. Otherwise, it is used as the size of the stack to
     * allocate, with zero indicating that the default size should be used.
     */
    size_t size;
} thread_stack_t;

/** Thread entry point type.
 * @param arg           Argument passed via kern_thread_create().
 * @return              Thread exit status. */
typedef int (*thread_entry_t)(void *arg);

/** Saved thread context. */
typedef struct thread_context {
    cpu_context_t cpu;                  /**< CPU context (register state, etc). */
    uint32_t ipl;                       /**< Interrupt priority level. */
} thread_context_t;

/** Handle value used to refer to the current thread. */
#define THREAD_SELF             INVALID_HANDLE

/** Thread object events. */
enum {
    THREAD_EVENT_DEATH          = 1,    /**< Wait for thread death. */
};

/** Thread priority values. */
enum {
    THREAD_PRIORITY_LOW         = 0,    /**< Low priority. */
    THREAD_PRIORITY_NORMAL      = 1,    /**< Normal priority. */
    THREAD_PRIORITY_HIGH        = 2,    /**< High priority. */
};

/** Thread interrupt priority level (IPL) definitions. */
enum {
    THREAD_IPL_EXCEPTION        = 14,   /**< Exception level. */
    THREAD_IPL_MAX              = 15,   /**< Maximum IPL (all interrupts blocked). */
};

/** Modes for kern_thread_set_ipl(). */
enum {
    /** Set the IPL to the given value regardless of its current value. */
    THREAD_SET_IPL_ALWAYS       = 0,

    /** Only set the IPL if it is higher than the current IPL. */
    THREAD_SET_IPL_RAISE        = 1,
};

extern status_t kern_thread_create(
    const char *name, thread_entry_t entry, void *arg,
    const thread_stack_t *stack, uint32_t flags, handle_t *_handle);
extern status_t kern_thread_open(thread_id_t id, handle_t *_handle);
extern status_t kern_thread_id(handle_t handle, thread_id_t *_id);
extern status_t kern_thread_security(handle_t handle, security_context_t *ctx);
extern status_t kern_thread_status(handle_t handle, int *_status, int *_reason);
extern status_t kern_thread_kill(handle_t handle);

extern status_t kern_thread_ipl(uint32_t *_ipl);
extern status_t kern_thread_set_ipl(uint32_t mode, uint32_t ipl, uint32_t *_prev_ipl);
extern status_t kern_thread_token(handle_t *_handle);
extern status_t kern_thread_set_token(handle_t handle);
extern status_t kern_thread_set_exception_handler(uint32_t code, exception_handler_t handler);
extern status_t kern_thread_set_exception_stack(const thread_stack_t *stack);

extern status_t kern_thread_exception(exception_info_t *info);
extern status_t kern_thread_sleep(nstime_t nsecs, nstime_t *_rem);
extern void kern_thread_exit(int status) __kernel_noreturn;

__KERNEL_EXTERN_C_END
