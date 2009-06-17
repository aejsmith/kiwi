/* Kiwi boot-time module loader
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
 * @brief		Boot-time module loader.
 */

#include <console/kprintf.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <bootmod.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>

/** Array of modules provided by the bootloader. */
bootmod_t *bootmod_array;

/** Number of modules in the boot module array. */
size_t bootmod_count;

/** Array of boot module handlers. */
static bootmod_handler_t bootmod_handlers[8];
static size_t bootmod_handler_count = 0;

/** Load a boot kernel module.
 * @param mod		Pointer to module structure.
 * @return		1 if loaded, 0 if module valid but cannot be loaded
 *			yet (dependencies, etc), -1 if module not this type. */
static int bootmod_kmod_handler(bootmod_t *mod) {
	char depbuf[MODULE_NAME_MAX + strlen(MODULE_EXTENSION) + 1];
	bootmod_t *dep;
	int ret;

	/* Check if this is a kernel module. */
	if(!module_check(mod->addr, mod->size)) {
		return -1;
	}

	/* Try to load the module and all dependencies. */
	ret = module_load(mod->addr, mod->size, depbuf);
	if(ret == 0) {
		return 1;
	} else if(ret != -ERR_DEP_MISSING) {
		fatal("Could not load kernel module %s (%d)", mod->name, ret);
	}

	/* We have a missing dependency, work out its name and check if we have
	 * it. */
	strcpy(depbuf + strlen(depbuf), MODULE_EXTENSION);
	dep = bootmod_lookup(depbuf);
	if(dep == NULL || !module_check(dep->addr, dep->size)) {
		fatal("Module %s depends on missing/invalid module %s", mod->name, depbuf);
	}

	return 0;
}

/** Find a module in the boot module array.
 *
 * Finds a module with the given name in the bood module array. This can be
 * used by, for example, the kernel module handler to check if a required
 * dependency actually exists.
 *
 * @param name		Name to look for.
 *
 * @return		Pointer to module, or NULL if not found.
 */
bootmod_t *bootmod_lookup(const char *name) {
	size_t i;

	for(i = 0; i < bootmod_count; i++) {
		if(strcmp(bootmod_array[i].name, name) == 0) {
			return &bootmod_array[i];
		}
	}

	return NULL;
}

/** Register a boot module handler.
 *
 * Registers a handler function for a certain type of boot module.
 *
 * @param handler	Handler to register.
 */
void bootmod_handler_register(bootmod_handler_t handler) {
	if(bootmod_handler_count >= 8) {
		fatal("Too many boot module handlers");
	}

	bootmod_handlers[bootmod_handler_count++] = handler;
}

/** Load all modules provided by the bootloader.
 *
 * Loads all modules provided by the bootloader. By the time this function
 * is called, the architecture or platform should have set the array pointer
 * and module count. This function keeps on looping over modules that it is
 * provided, attempting to load anything that hasn't already been succesfully
 * loaded, until it can do no more. This lets two things happen: first, it
 * allows kernel modules to be loaded in dependency order. Secondly, it lets
 * kernel modules register handlers for other types of modules that may be
 * passed to the kernel, and ensures these handlers will get called on things
 * that haven't been loaded.
 */
void bootmod_load(void) {
	size_t i, j, count;
	int ret;

	/* Check that we have any modules. The kernel cannot do anything
	 * without modules, so there must be some. */
	if(bootmod_count == 0) {
		fatal("No modules were provided, cannot continue");
	}

	/* Add the kernel module handler. */
	bootmod_handler_register(bootmod_kmod_handler);

	/* Keep on looping over the modules we have until nothing else can be
	 * done. */
	while(true) {
		count = 0;

		/* Loop through all modules that haven't been loaded. */
		for(i = 0; i < bootmod_count; i++) {
			if(bootmod_array[i].loaded) {
				continue;
			}

			/* For each handler check if we can do something. */
			for(j = 0; j < bootmod_handler_count; j++) {
				ret = bootmod_handlers[j](&bootmod_array[i]);
				if(ret == 1) {
					kprintf(LOG_DEBUG, "bootmod: loaded module %s (addr: 0x%p, size: %u)\n",
					        bootmod_array[i].name, bootmod_array[i].addr,
					        bootmod_array[i].size);
					bootmod_array[i].loaded = true;
					count++;
					break;
				} else if(ret == 0) {
					break;
				}
			}
		}

		/* If nothing was done in this iteration, we can finish now. */
		if(count == 0) {
			break;
		}
	}


	/* Free the data for the modules. */
	for(i = 0; i < bootmod_count; i++) {
		if(!bootmod_array[i].loaded) {
			kprintf(LOG_NORMAL, "bootmod: warning: module %s was not handled\n",
			        bootmod_array[i].name);
		}
		kfree(bootmod_array[i].name);
		kfree(bootmod_array[i].addr);
	}
	kfree(bootmod_array);
}
