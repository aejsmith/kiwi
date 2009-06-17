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

#include "loader_priv.h"

/** Replace the current process with a new binary.
 *
 * Replaces the current process with a new binary. This is done in several
 * steps.
 *  - Load the binary into a new address space.
 *  - Terminate all threads except the current thread.
 *  - Replace the current address space with the new one.
 *  - Begin executing the new binary.
 * To perform the second and third steps, process_reset() is called. If the
 * new binary runs under the same subsystem as the old binary, then this will
 * call the process_reset callback for the subsystem. Otherwise, it calls
 * process_destroy for the old subsystem, and process_init for the new one.
 * This allows, for example, the POSIX subsystem to preserve file descriptors
 * across an execve() call for another POSIX binary.
 *
 * When successful, this function does not return to the calling kernel
 * function. This means that several assumptions must be made about the
 * arguments it is passed. It will release the filesystem node given, and both
 * the contents of the arrays given and the arrays themselves will be freed.
 *
 * @param node		Node referring to the binary to load.
 * @param args		Arguments to pass to the new process (how these are
 *			passed in, or whether they are passed at all, are
 *			dependent on the subsystem in use). This can be NULL.
 * @param environ	Environment variables for the new process (same rules
 *			apply as for arguments). This can be NULL.
 * @param sem		If this argument is not NULL, it should point to a
 *			semaphore that will be upped if this function is
 *			successful, just before it enters the new program.
 *			This allows, for example, something to create a new
 *			process which runs this function, and get notified
 *			if it successfully completes.
 *
 * @return		Does not return on success; if it does return it will
 *			be for a failure, in which case the return value will
 *			be a negative error code.
 */
int loader_binary_load(vfs_node_t *node, const char **args, const char **environ, semaphore_t *sem) {
	loader_binary_t binary;

	binary.node = node;
	binary.args = args;
	binary.environ = environ;

	/* Attempt to match the binary to a type. */
	binary.type = loader_type_match(node);
	if(binary.type == NULL) {
		return -ERR_OBJ_TYPE_INVAL;
	}

	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(loader_binary_load);
