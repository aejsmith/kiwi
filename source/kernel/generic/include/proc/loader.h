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

#ifndef	__PROC_LOADER_H
#define __PROC_LOADER_H

#include <io/vfs.h>

#include <mm/aspace.h>

#include <types/list.h>

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

/** Executable loader type definition structure. */
typedef struct loader_type {
	list_t header;			/**< Link to types list. */
	const char *name;		/**< Name of type. */

	/** Check whether a binary matches this type.
	 * @param node		Filesystem node referring to the binary.
	 * @return		Whether the binary is of this type. */
	bool (*check)(vfs_node_t *node);

	/** Load a binary into an address space.
	 * @note		This should also set the entry pointer in the
	 *			binary structure.
	 * @param binary	Binary loader data structure.
	 * @return		0 on success, negative error code on failure. */
	int (*load)(loader_binary_t *binary);

	/** Finish binary loading, after address space is switched.
	 * @note		It is the job of this function to copy
	 *			arguments and environment to the stack (the
	 *			stack pointer is set in the binary structure
	 *			when this is called).
	 * @param binary	Binary loader data structure.
	 * @return		0 on success, negative error code on failure.
	 *			Be warned that returning a failure at this
	 *			point in the execution process will result
	 *			in the process being terminated if the
	 *			execution is replacing an existing process. */
	int (*finish)(loader_binary_t *binary);

	/** Clean up data stored in a binary structure.
	 * @param binary	Binary loader data structure. */
	void (*cleanup)(loader_binary_t *binary);
} loader_type_t;

extern int loader_binary_load(vfs_node_t *node, char **args, char **environ, semaphore_t *sem);

extern int loader_type_register(loader_type_t *type);

#endif /* __PROC_LOADER_H */
