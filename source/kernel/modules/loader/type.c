/* Kiwi executable type manager
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
 * @brief		Executable type manager.
 */

#include <sync/mutex.h>

#include "loader_priv.h"

/** List of known executable types. */
static LIST_DECLARE(exec_type_list);
static MUTEX_DECLARE(exec_type_list_lock);

/** Match a binary to an executable type.
 * @param node		VFS node for the binary.
 * @return		Pointer to type if matched, NULL if not. */
loader_type_t *loader_type_match(vfs_node_t *node) {
	loader_type_t *type;

	mutex_lock(&exec_type_list_lock, 0);

	LIST_FOREACH(&exec_type_list, iter) {
		type = list_entry(iter, loader_type_t, header);

		if(type->check(node)) {
			mutex_unlock(&exec_type_list_lock);
			return type;
		}
	}

	mutex_unlock(&exec_type_list_lock);
	return NULL;
}

/** Register an executable type.
 *
 * Registers an executable type with the program loader.
 *
 * @param type		Executable type to add.
 */
int loader_type_register(loader_type_t *type) {
	if(!type->name || !type->check || !type->load || !type->finish) {
		return -ERR_PARAM_INVAL;
	}

	list_init(&type->header);

	mutex_lock(&exec_type_list_lock, 0);
	list_append(&exec_type_list, &type->header);
	mutex_unlock(&exec_type_list_lock);

	dprintf("loader: registered executable type 0x%p(%s)\n", type, type->name);
	return 0;
}
MODULE_EXPORT(loader_type_register);
