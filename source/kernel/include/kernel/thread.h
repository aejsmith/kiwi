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

#include <kernel/object.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Thread object events. */
#define THREAD_EVENT_DEATH	0	/**< Wait for thread death. */

/** Actions for kern_thread_control(). */
#define THREAD_SET_TLS_ADDR	1	/**< Set TLS base address (calling thread only). */

/** Maximum length of a thread name. */
#define THREAD_NAME_MAX		32

/** Thread priority values. */
#define THREAD_PRIORITY_LOW	0	/**< Low priority. */
#define THREAD_PRIORITY_NORMAL	1	/**< Normal priority. */
#define THREAD_PRIORITY_HIGH	2	/**< High priority. */

extern status_t kern_thread_create(const char *name, void *stack, size_t stacksz,
	void (*func)(void *), void *arg, handle_t *handlep);
extern status_t kern_thread_open(thread_id_t id, handle_t *handlep);
extern thread_id_t kern_thread_id(handle_t handle);
extern status_t kern_thread_control(handle_t handle, int action, const void *in, void *out);
extern status_t kern_thread_status(handle_t handle, int *statusp);
extern void kern_thread_exit(int status) __attribute__((noreturn));
extern status_t kern_thread_usleep(useconds_t us, useconds_t *remp);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_THREAD_H */
