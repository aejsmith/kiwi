/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		Kernel object functions/definitions.
 */

#ifndef __KERNEL_OBJECT_H
#define __KERNEL_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef KERNEL
# include <public/types.h>
#else
# include <kernel/types.h>
#endif

/** Object type ID definitions. */
#define OBJECT_TYPE_FILE	1	/**< File. */
#define OBJECT_TYPE_DIR		2	/**< Directory. */
#define OBJECT_TYPE_DEVICE	3	/**< Device. */
#define OBJECT_TYPE_PROCESS	4	/**< Process. */
#define OBJECT_TYPE_THREAD	5	/**< Thread. */
#define OBJECT_TYPE_PORT	6	/**< IPC port. */
#define OBJECT_TYPE_CONNECTION	7	/**< IPC connection. */
#define OBJECT_TYPE_SEMAPHORE	8	/**< Semaphore. */
#define OBJECT_TYPE_SHM		9	/**< Shared memory area. */

/** Handle behaviour flags. */
#define HANDLE_INHERITABLE	(1<<0)	/**< Handle will be inherited by child processes. */

extern int SYSCALL(object_type)(handle_t handle);
extern status_t SYSCALL(object_wait)(handle_t handle, int event, useconds_t timeout);
extern status_t SYSCALL(object_wait_multiple)(handle_t *handle, int *events, size_t count, useconds_t timeout);
extern status_t SYSCALL(handle_get_flags)(handle_t handle, int *flagsp);
extern status_t SYSCALL(handle_set_flags)(handle_t handle, int flags);
extern status_t SYSCALL(handle_close)(handle_t handle);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_OBJECT_H */
