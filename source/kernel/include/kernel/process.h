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

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Arguments passed to the userspace loader. */
typedef struct process_args {
	char *path;			/**< Path to program. */
	char **args;			/**< Argument array. */
	char **env;			/**< Environment variable array. */
	int args_count;			/**< Number of entries in argument array (excluding NULL). */
	int env_count;			/**< Number of entries in environment array (excluding NULL). */
	void *load_base;		/**< Base address that kernel library was loaded to. */
} process_args_t;

/** Expected path to libkernel. */
#define LIBKERNEL_PATH		"/system/libraries/libkernel.so"

/** Process object events. */
#define PROCESS_EVENT_DEATH	0	/**< Wait for process death. */

/** Process creation flags. */
#define PROCESS_CREATE_CLONE	(1<<0)	/**< Inherit extra information from the parent. */

/** Actions for kern_process_control(). */
#define PROCESS_LOADED		1	/**< Signal that process is loaded (calling process only). */

/** Process exit reason codes. */
#define EXIT_REASON_NORMAL	0	/**< Normal exit (status is exit code). */
#define EXIT_REASON_SIGNAL	1	/**< Exited due to a signal (status is signal number). */

/** Process priority classes. */
#define PRIORITY_CLASS_LOW	0	/**< Low priority. */
#define PRIORITY_CLASS_NORMAL	1	/**< Normal priority. */
#define PRIORITY_CLASS_HIGH	2	/**< High priority. */

extern status_t kern_process_create(const char *path, const char *const args[],
	const char *const env[], int flags, handle_t map[][2], int count,
	handle_t *handlep);
extern status_t kern_process_replace(const char *path, const char *const args[],
	const char *const env[], handle_t map[][2], int count);
extern status_t kern_process_clone(void (*func)(void *), void *arg, void *sp,
	handle_t *handlep);
extern status_t kern_process_open(process_id_t id, handle_t *handlep);
extern process_id_t kern_process_id(handle_t handle);
extern status_t kern_process_control(handle_t handle, int action, const void *in, void *out);
extern status_t kern_process_status(handle_t handle, int *statusp, int *reasonp);
extern void kern_process_exit(int status) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_PROCESS_H */
