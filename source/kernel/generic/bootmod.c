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

/** Array of boot modules. */
static bootmod_t *bootmod_array;
static size_t bootmod_count;

/** Find a module in the boot module array.
 * @param name		Name to look for.
 * @return		Pointer to module, or NULL if not found. */
static bootmod_t *bootmod_lookup(const char *name) {
	size_t i;

	for(i = 0; i < bootmod_count; i++) {
		if(strcmp(bootmod_array[i].name, name) == 0) {
			return &bootmod_array[i];
		}
	}

	return NULL;
}

/** Load a boot kernel module and any dependencies.
 * @param mod		Pointer to module structure.
 * @return		True if module was a kernel module, false if not. */
static bool bootmod_load_kmod(bootmod_t *mod) {
	char depbuf[MODULE_NAME_MAX + strlen(MODULE_EXTENSION) + 1];
	bootmod_t *dep;
	int ret;

	/* Check if this is a kernel module. */
	if(!module_check(mod->addr, mod->size)) {
		return false;
	}

	/* Try to load the module and all dependencies. */
	while(true) {
		ret = module_load(mod->addr, mod->size, depbuf);
		if(ret == 0) {
			kprintf(LOG_DEBUG, "bootmod: loaded module %s (addr: 0x%p, size: %u)\n",
				mod->name, mod->addr, mod->size);
			mod->loaded = true;
			return true;
		} else if(ret != -ERR_DEP_MISSING) {
			fatal("Could not load module %s: %d", mod->name, ret);
		}

		/* We have a missing dependency, work out its name, check if
		 * we have it and attempt to load it. */
		strcpy(depbuf + strlen(depbuf), MODULE_EXTENSION);
		dep = bootmod_lookup(depbuf);
		if(dep == NULL) {
			fatal("Module %s depends on missing module %s", mod->name, depbuf);
		}

		/* Attempt to load the dependency. Recursion! */
		if(!bootmod_load_kmod(dep)) {
			fatal("Dependency %s of %s is not a kernel module", depbuf, mod->name);
		}
	}
}

/** Load all kernel modules provided at boot. */
void bootmod_load(void) {
	size_t i;

	/* Ask the architecture/platform to give us all the modules it has. */
	bootmod_count = bootmod_get(&bootmod_array);
	if(bootmod_count == 0) {
		fatal("No modules were provided, cannot continue");
	}

	/* Load all the modules. */
	for(i = 0; i < bootmod_count; i++) {
		/* Ignore already loaded modules (may be already loaded due
		 * to dependency loading for another module). */
		if(bootmod_array[i].loaded) {
			continue;
		}

		/* First check if this is a kernel module. */
		if(bootmod_load_kmod(&bootmod_array[i])) {
			continue;
		} else {
			fatal("Unknown module: %s", bootmod_array[i].name);
		}
	}

	/* Free the data for the modules. */
	for(i = 0; i < bootmod_count; i++) {
		kfree(bootmod_array[i].name);
		kfree(bootmod_array[i].addr);
	}
	kfree(bootmod_array);
}
