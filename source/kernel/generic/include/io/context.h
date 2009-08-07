/* Kiwi I/O context functions
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
 * @brief		I/O context functions.
 */

#ifndef __IO_CONTEXT_H
#define __IO_CONTEXT_H

#include <io/vfs.h>

#include <sync/mutex.h>

/** Structure containing an I/O context. */
typedef struct io_context {
	mutex_t lock;			/**< Lock to protect context. */
	vfs_node_t *root_dir;		/**< Root directory. */
	vfs_node_t *curr_dir;		/**< Current working directory. */
	list_t async_requests;		/**< Current in-progress asynchronous I/O requests. */
} io_context_t;

extern int io_context_init(io_context_t *context, io_context_t *parent);
extern void io_context_destroy(io_context_t *context);
extern int io_context_setcwd(io_context_t *context, vfs_node_t *node);
extern int io_context_setroot(io_context_t *context, vfs_node_t *node);

#endif /* __IO_CONTEXT_H */
