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
 * @brief		Process management functions.
 */

#ifndef __KERNEL_PROCESS_H
#define __KERNEL_PROCESS_H

#include <kernel/security.h>
#include <kernel/thread.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Handle value used to refer to the current process. */
#define PROCESS_SELF		INVALID_HANDLE

/** Process object events. */
#define PROCESS_EVENT_DEATH	1	/**< Wait for process death. */

/** Process exit reason codes. */
#define EXIT_REASON_NORMAL	1	/**< Normal exit (status is exit code). */
#define EXIT_REASON_SIGNAL	2	/**< Exited due to a signal (status is signal number). */

/** Process priority classes. */
#define PRIORITY_CLASS_LOW	0	/**< Low priority. */
#define PRIORITY_CLASS_NORMAL	1	/**< Normal priority. */
#define PRIORITY_CLASS_HIGH	2	/**< High priority. */

/** Process creation flags. */
#define PROCESS_CREATE_CRITICAL	(1<<0)	/**< Process is a critical system process. */

extern status_t kern_process_create(const char *path, const char *const args[],
	const char *const env[], uint32_t flags, handle_t token, handle_t map[][2],
	ssize_t count, handle_t *handlep);
extern status_t kern_process_exec(const char *path, const char *const args[],
	const char *const env[], uint32_t flags, handle_t token, handle_t map[][2],
	ssize_t count);
extern status_t kern_process_clone(handle_t *handlep);
extern status_t kern_process_open(process_id_t id, handle_t *handlep);
extern process_id_t kern_process_id(handle_t handle);
extern status_t kern_process_security(handle_t handle, security_context_t *ctx);
extern status_t kern_process_status(handle_t handle, int *statusp, int *reasonp);

extern status_t kern_process_token(handle_t *handlep);
extern status_t kern_process_set_token(handle_t handle);
extern void kern_process_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PROCESS_H */
