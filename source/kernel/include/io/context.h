/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <sync/rwlock.h>

struct fs_node;

/** Structure containing an I/O context. */
typedef struct io_context {
	rwlock_t lock;			/**< Lock to protect context. */
	struct fs_node *root_dir;	/**< Root directory. */
	struct fs_node *curr_dir;	/**< Current working directory. */
} io_context_t;

extern void io_context_init(io_context_t *context, io_context_t *parent);
extern void io_context_destroy(io_context_t *context);
extern status_t io_context_setcwd(io_context_t *context, struct fs_node *node);
extern status_t io_context_setroot(io_context_t *context, struct fs_node *node);

#endif /* __IO_CONTEXT_H */
