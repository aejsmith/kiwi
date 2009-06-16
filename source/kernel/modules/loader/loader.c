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

#include <fatal.h>
#include <init.h>

#include "loader_priv.h"

/** Thread to load initial userspace program.
 * @param arg		Pointer to waiting semaphore. */
static void loader_init_thread(void *arg) {
	vfs_node_t *node;
	int ret;

	/* TODO: Get path from configuration system. */
	ret = vfs_node_lookup(NULL, "init", &node);
	if(ret != 0) {
		fatal("Could not find initialization program");
	}

	ret = loader_binary_load(node, NULL, NULL, (semaphore_t *)arg);
	fatal("Could not load initialization program");
}

/** Initialization callback to load the initial userspace program.
 * @param data1		First callback data argument (ignored).
 * @param data2		Second callback data argument (ignored). */
static void loader_load_init(void *data1, void *data2) {
	SEMAPHORE_DECLARE(semaphore, 0);
	process_t *process;
	thread_t *thread;
	int ret;

	ret = process_create("init", kernel_proc, PRIORITY_SYSTEM, PROCESS_CRITICAL, NULL, &process);
	if(ret != 0) {
		fatal("Could not create userspace initialization process");
	}

	ret = thread_create("init", process, 0, loader_init_thread, &semaphore, &thread);
	if(ret != 0) {
		fatal("Could not create userspace initialization thread");
	}
	thread_run(thread);

	/* Wait for completion of the process. */
	semaphore_down(&semaphore, 0);
}

CALLBACK_DECLARE(loader_load_init_callback, loader_load_init, NULL);

/** Program loader initialization function.
 * @return		0 on success, negative error code on failure. */
static int loader_init(void) {
	/* Register built-in types. This shouldn't fail. */
	if(loader_type_register(&loader_elf_type) != 0) {
		fatal("Could not register built-in executable types");
	}

	/* Register an initialization callback to load the first userspace
	 * binary. */
	callback_add(&init_completion_cb_list, &loader_load_init_callback);
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
MODULE_DEPS("vfs");
