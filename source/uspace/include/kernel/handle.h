/* Kiwi handle functions
 * Copyright (C) 2009 Alex Smith
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
 * @brief		Handle functions.
 */

#ifndef __KERNEL_HANDLE_H
#define __KERNEL_HANDLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <kernel/types.h>

/** Handle type ID definitions. */
#define HANDLE_TYPE_FILE	1	/**< File. */
#define HANDLE_TYPE_DIR		2	/**< Directory. */
#define HANDLE_TYPE_PROCESS	3	/**< Process. */
#define HANDLE_TYPE_THREAD	4	/**< Thread. */
#define HANDLE_TYPE_DEVICE	5	/**< Device. */

extern int handle_close(handle_t handle);
extern int handle_type(handle_t handle);
extern int handle_wait(handle_t handle, int event, timeout_t timeout);
extern int handle_wait_multiple(handle_t *handles, int *events, size_t count, timeout_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_HANDLE_H */
