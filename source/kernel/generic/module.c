/* Kiwi kernel module loader
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
 * @brief		Kernel module loader.
 */

#include <console/kprintf.h>

#include <sync/mutex.h>

#include <elf.h>
#include <kdbg.h>
#include <module.h>

/** List of loaded modules. */
static LIST_DECLARE(module_list);
static MUTEX_DECLARE(module_list_lock);

/** Load a kernel module.
 *
 * Loads a kernel module from a memory buffer. The buffer should contain a
 * valid ELF image. If any dependencies on this module are not met, the name
 * of the first unmet dependency is stored in the buffer provided, which should
 * be MODULE_NAME_MAX bytes long. The intended usage of this function is to
 * keep on calling it and loading each unmet dependency it specifies until it
 * succeeds.
 *
 * @param image		Pointer to ELF image in memory.
 * @param size		Size of image.
 * @param dep		Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX bytes long).
 *
 * @return		0 on success, negative error code on failure. If a
 *			required dependency is not loaded, the ERR_DEP_MISSING
 *			error code is returned.
 */
int module_load(void *image, size_t size, char *dep) {
	return 0;
}

/** Print a list of loaded kernel modules.
 *
 * Prints a list of currently loaded kernel modules and information about
 * them.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		Always returns KDBG_OK.
 */
int kdbg_cmd_modules(int argc, char **argv) {
	module_t *module;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all loaded kernel modules.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "Name             Count Description\n");
	kprintf(LOG_NONE, "====             ===== ===========\n");

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		kprintf(LOG_NONE, "%-16s %-5d %s\n", module->name,
		        refcount_get(&module->count), module->description);
	}

	return KDBG_OK;
}
