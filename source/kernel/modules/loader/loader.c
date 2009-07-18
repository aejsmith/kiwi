/* Kiwi program loader
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
 * @brief		Program loader/dynamic linker.
 */

#include <proc/process.h>
#include <proc/thread.h>

#include <bootmod.h>
#include <fatal.h>

#include "loader_priv.h"

/** Thread to load a binary provided at boot.
 * @param arg1		Pointer to VFS node for binary.
 * @param arg2		Pointer to semaphore to up upon successful load. */
static void loader_bootmod_thread(void *arg1, void *arg2) {
	semaphore_t *sem = arg2;
	vfs_node_t *node = arg1;
	int ret;

	ret = loader_binary_load(node, NULL, NULL, sem);
	fatal("Failed to load binary '%s' (%d)", node->name, ret);
}

/** Executable boot module handler.
 * @param mod		Boot module structure.
 * @return		1 if loaded, 0 if module valid but cannot be loaded
 *			yet (dependencies, etc), -1 if module not this type. */
static int loader_bootmod_handler(bootmod_t *mod) {
	SEMAPHORE_DECLARE(semaphore, 0);
	vfs_node_t *node;
	thread_t *thread;
	process_t *proc;
	int ret;

	/* Create a node from the module's data. */
	ret = vfs_node_create_from_memory(mod->name, (const void *)mod->addr, mod->size, &node);
	if(ret != 0) {
		fatal("Could not create VFS node from module data (%d)", ret);
	}

	/* Attempt to match the node to a type. */
	if(!loader_type_match(node)) {
		vfs_node_release(node);
		return -1;
	}

	/* Create process to load the binary. Does not require an address space
	 * to begin with as loader_binary_load() will create one. */
	ret = process_create(mod->name, kernel_proc, PRIORITY_SYSTEM, PROCESS_NOASPACE, &proc);
	if(ret != 0) {
		fatal("Could not create process to load binary (%d)", ret);
	}

	ret = thread_create(mod->name, proc, 0, loader_bootmod_thread, node, &semaphore, &thread);
	if(ret != 0) {
		fatal("Could not create thread to load binary (%d)", ret);
	}
	thread_run(thread);

	/* Wait for the binary to load. The node is released for us by
	 * loader_binary_load(). */
	semaphore_down(&semaphore, 0);
	return 1;
}

/** Program loader initialization function.
 * @return		0 on success, negative error code on failure. */
static int loader_init(void) {
	/* Register built-in types. This shouldn't fail. */
	if(loader_type_register(&loader_elf_type) != 0) {
		fatal("Could not register built-in executable types");
	}

	/* Register a boot module handler to run binaries. */
	bootmod_handler_register(loader_bootmod_handler);
	return 0;
}

/** Program loader unload function.
 * @return		0 on success, negative error code on failure. */
static int loader_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("loader");
MODULE_DESC("Program loader and dynamic linker.");
MODULE_FUNCS(loader_init, loader_unload);
