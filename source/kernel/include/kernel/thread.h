/*
 * Copyright (C) 2009-2014 Alex Smith
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
 * @brief               Thread management functions.
 */

#ifndef __KERNEL_THREAD_H
#define __KERNEL_THREAD_H

#include <kernel/context.h>
#include <kernel/exception.h>
#include <kernel/limits.h>
#include <kernel/object.h>
#include <kernel/security.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    cpu_context_t cpu;              /**< CPU context (register state, etc). */
    unsigned ipl;                   /**< Interrupt priority level. */
} thread_context_t;

/** Handle value used to refer to the current thread. */
#define THREAD_SELF             INVALID_HANDLE

/** Thread object events. */
#define THREAD_EVENT_DEATH      1   /**< Wait for thread death. */

/** Thread priority values. */
#define THREAD_PRIORITY_LOW     0   /**< Low priority. */
#define THREAD_PRIORITY_NORMAL  1   /**< Normal priority. */
#define THREAD_PRIORITY_HIGH    2   /**< High priority. */

/** Thread interrupt priority level (IPL) definitions. */
#define THREAD_IPL_EXCEPTION    14  /**< Exception level. */
#define THREAD_IPL_MAX          15  /**< Maximum IPL (all interrupts blocked). */

extern status_t kern_thread_create(
    const char *name, thread_entry_t entry, void *arg,
    const thread_stack_t *stack, uint32_t flags, handle_t *_handle);
extern status_t kern_thread_open(thread_id_t id, handle_t *_handle);
extern thread_id_t kern_thread_id(handle_t handle);
extern status_t kern_thread_security(handle_t handle, security_context_t *ctx);
extern status_t kern_thread_status(handle_t handle, int *_status, int *_reason);
extern status_t kern_thread_kill(handle_t handle);

extern status_t kern_thread_ipl(unsigned *_ipl);
extern status_t kern_thread_set_ipl(unsigned ipl);
extern status_t kern_thread_token(handle_t *_handle);
extern status_t kern_thread_set_token(handle_t handle);
extern status_t kern_thread_set_exception_handler(unsigned code, exception_handler_t handler);
extern status_t kern_thread_set_exception_stack(const thread_stack_t *stack);

extern status_t kern_thread_raise(exception_info_t *info);
extern status_t kern_thread_sleep(nstime_t nsecs, nstime_t *_rem);
extern void kern_thread_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_THREAD_H */
