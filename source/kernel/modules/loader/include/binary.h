/* Kiwi executable loader
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
 * @brief		Executable loader.
 */

#ifndef	__LOADER_BINARY_H
#define __LOADER_BINARY_H

#include <fs/node.h>

#include <mm/aspace.h>

#include <sync/semaphore.h>

struct loader_type;

/** Structure storing data used by the executable loader. */
typedef struct loader_binary {
	vfs_node_t *node;		/**< Filesystem node referring to the binary. */
	struct loader_type *type;	/**< Pointer to executable type. */
	void *data;			/**< Data used by the executable type. */

	aspace_t *aspace;		/**< Address space that the binary is being loaded into. */
	ptr_t stack;			/**< Stack pointer for the initial thread. */
	ptr_t entry;			/**< Entry point for the binary. */

	char **args;			/**< Argument array. */
	char **environ;			/**< Environment variable array. */
} loader_binary_t;

extern int loader_binary_load(vfs_node_t *node, char **args, char **environ, semaphore_t *sem);

#endif /* __LOADER_BINARY_H */
