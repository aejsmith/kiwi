/*
 * Copyright (C) 2013 Alex Smith
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
 * @brief		Kernel object functions/definitions.
 */

#ifndef __KERNEL_OBJECT_H
#define __KERNEL_OBJECT_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Object type ID definitions. */
#define OBJECT_TYPE_FILE	1	/**< File. */
#define OBJECT_TYPE_DEVICE	2	/**< Device. */
#define OBJECT_TYPE_PROCESS	3	/**< Process. */
#define OBJECT_TYPE_THREAD	4	/**< Thread. */
#define OBJECT_TYPE_PORT	5	/**< IPC port. */
#define OBJECT_TYPE_CONNECTION	6	/**< IPC connection. */
#define OBJECT_TYPE_SEMAPHORE	7	/**< Semaphore. */
#define OBJECT_TYPE_AREA	8	/**< Memory area. */
#define OBJECT_TYPE_TIMER	9	/**< Timer. */

/** Handle link behaviour flags. */
#define HANDLE_INHERITABLE	(1<<0)	/**< Handle will be inherited by child processes. */

/** Actions for kern_handle_control(). */
#define HANDLE_GET_LFLAGS	1	/**< Get handle table link flags. */
#define HANDLE_SET_LFLAGS	2	/**< Set handle table link flags. */
#define HANDLE_GET_FLAGS	3	/**< Get handle flags (object type-specific). */
#define HANDLE_SET_FLAGS	4	/**< Set handle flags (object type-specific). */
#define HANDLE_GET_RIGHTS	5	/**< Get handle rights. */
#define HANDLE_SET_RIGHTS	6	/**< Set handle rights. */

/** Type used to store a set of object rights. */
typedef uint32_t object_rights_t;

/** Details of an object event to wait for. */
typedef struct object_event {
	handle_t handle;		/**< Handle to wait on. */
	int event;			/**< Event to wait for. */
	bool signalled;			/**< Whether the event was signalled. */
} object_event_t;

extern int kern_object_type(handle_t handle);
extern status_t kern_object_wait(object_event_t *events, size_t count, useconds_t timeout);

extern status_t kern_handle_control(handle_t handle, int action, int arg, int *outp);
extern status_t kern_handle_duplicate(handle_t handle, handle_t dest, bool force, handle_t *newp);
extern status_t kern_handle_close(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_OBJECT_H */
