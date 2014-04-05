/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief		Kernel object management.
 */

#ifndef __KERNEL_OBJECT_H
#define __KERNEL_OBJECT_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cpu_context;

/** Value used to refer to an invalid handle.
 * @note		This is used to mean various things, for example with
 *			thread/process functions it refers to the current
 *			thread/process rather than one referred to by a handle. */
#define INVALID_HANDLE          (-1)

/** Object type ID definitions. */
#define OBJECT_TYPE_PROCESS	1	/**< Process (transferrable). */
#define OBJECT_TYPE_THREAD	2	/**< Thread (transferrable). */
#define OBJECT_TYPE_TOKEN	3	/**< Security Token (transferrable). */
#define OBJECT_TYPE_TIMER	4	/**< Timer (transferrable). */
#define OBJECT_TYPE_WATCHER	5	/**< Watcher (non-transferrable). */
#define OBJECT_TYPE_AREA	6	/**< Memory Area (transferrable). */
#define OBJECT_TYPE_FILE	7	/**< File (transferrable). */
#define OBJECT_TYPE_PORT	8	/**< Port (transferrable). */
#define OBJECT_TYPE_CONNECTION	9	/**< Connection (non-transferrable). */
#define OBJECT_TYPE_SEMAPHORE	10	/**< Semaphore (transferrable). */

/** Flags for a handle table entry. */
#define HANDLE_INHERITABLE	(1<<0)	/**< Handle will be inherited by child processes. */

/** Details of an object event to wait for. */
typedef struct object_event {
	handle_t handle;		/**< Handle to wait on. */
	unsigned event;			/**< Event to wait for. */
	unsigned long data;		/**< Integer data associated with the event. */
	bool signalled;			/**< Whether the event was signalled. */
} object_event_t;

/** Behaviour flags for kern_object_wait(). */
#define OBJECT_WAIT_ALL		(1<<0)	/**< Wait for all the specified events to occur. */

extern status_t kern_object_type(handle_t handle, unsigned *typep);
extern status_t kern_object_wait(object_event_t *events, size_t count,
	uint32_t flags, nstime_t timeout);

extern status_t kern_handle_flags(handle_t handle, uint32_t *flagsp);
extern status_t kern_handle_set_flags(handle_t handle, uint32_t flags);
extern status_t kern_handle_duplicate(handle_t handle, handle_t dest,
	handle_t *newp);
extern status_t kern_handle_close(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_OBJECT_H */
