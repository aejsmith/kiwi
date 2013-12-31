/*
 * Copyright (C) 2009-2013 Alex Smith
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
 * @brief		Thread management functions.
 */

#ifndef __KERNEL_THREAD_H
#define __KERNEL_THREAD_H

#include <kernel/limits.h>
#include <kernel/object.h>
#include <kernel/security.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Thread entry information. */
typedef struct thread_entry {
	void (*func)(void *);		/**< Entry point for the thread. */
	void *arg;			/**< Argument to the entry function. */

	/**
	 * Base of stack.
	 *
	 * Base address of the stack area for the process. The kernel deals with
	 * setting the stack pointer within the specified area. If NULL, a stack
	 * will be allocated by the kernel, and will be freed automatically
	 * when the thread terminates. If not NULL, it is the responsibility of
	 * the program to free the stack after the thread terminates.
	 */
	void *stack;

	/**
	 * Size of the stack.
	 *
	 * If stack is not NULL, then this should be the non-zero size of the
	 * provided stack. Otherwise, it is used as the size of the stack to
	 * create, with zero indicating that the default size should be used.
	 */
	size_t stack_size;
} thread_entry_t;

/** Handle value used to refer to the current thread. */
#define THREAD_SELF		INVALID_HANDLE

/** Thread object events. */
#define THREAD_EVENT_DEATH	1	/**< Wait for thread death. */

/** Thread priority values. */
#define THREAD_PRIORITY_LOW	0	/**< Low priority. */
#define THREAD_PRIORITY_NORMAL	1	/**< Normal priority. */
#define THREAD_PRIORITY_HIGH	2	/**< High priority. */

extern status_t kern_thread_create(const char *name,
	const thread_entry_t *entry, uint32_t flags, handle_t *handlep);
extern status_t kern_thread_open(thread_id_t id, handle_t *handlep);
extern thread_id_t kern_thread_id(handle_t handle);
extern status_t kern_thread_security(handle_t handle, security_context_t *ctx);
extern status_t kern_thread_status(handle_t handle, int *statusp);
extern status_t kern_thread_port(handle_t handle, int32_t id,
	handle_t *handlep);

extern status_t kern_thread_token(handle_t *handlep);
extern status_t kern_thread_set_token(handle_t handle);
extern status_t kern_thread_sleep(nstime_t nsecs, nstime_t *remp);
extern void kern_thread_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_THREAD_H */
