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

#include <arch/memmap.h>

#include <console/kprintf.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/safe.h>
#include <mm/vmem.h>

#include <sync/mutex.h>

#include <errors.h>
#include <kdbg.h>
#include <module.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** List of loaded modules. */
static LIST_DECLARE(module_list);
static MUTEX_DECLARE(module_lock, 0);

/** Arena used for allocating memory for kernel modules. */
static vmem_t *module_arena;
#ifdef KERNEL_MODULE_BASE
static vmem_t *module_raw_arena;
#endif

/** Initialise the kernel module allocation arena. */
static void module_mem_init(void) {
#ifdef KERNEL_MODULE_BASE
	module_raw_arena = vmem_create("module_raw_arena", KERNEL_MODULE_BASE,
	                               KERNEL_MODULE_SIZE, PAGE_SIZE, NULL,
	                               NULL, NULL, 0, MM_FATAL);
	module_arena = vmem_create("module_arena", NULL, 0, PAGE_SIZE,
	                           kheap_anon_afunc, kheap_anon_ffunc,
	                           module_raw_arena, 0, MM_FATAL);
#else
	module_arena = vmem_create("module_arena", NULL, 0, PAGE_SIZE,
	                           kheap_anon_afunc, kheap_anon_ffunc,
	                           &kheap_va_arena, 0, MM_FATAL);
#endif
}

/** Allocate memory suitable to hold a kernel module.
 * @param size		Size of the allocation.
 * @param mmflag	Allocation flags.
 * @return		Address allocated or NULL if no available memory. */
void *module_mem_alloc(size_t size, int mmflag) {
	/* Create the arenas if they have not been created. */
	if(!module_arena) {
		module_mem_init();
	}

	return (void *)((ptr_t)vmem_alloc(module_arena, ROUND_UP(size, PAGE_SIZE), mmflag));
}

/** Free memory holding a module.
 * @param base		Base of the allocation.
 * @param size		Size of the allocation. */
static void module_mem_free(void *base, size_t size) {
	vmem_free(module_arena, (vmem_resource_t)((ptr_t)base), ROUND_UP(size, PAGE_SIZE));
}

/** Find a module in the module list.
 * @param name		Name of module to find.
 * @return		Pointer to module structure, NULL if not found. */
static module_t *module_find(const char *name) {
	module_t *module;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		if(strcmp(module->name, name) == 0) {
			return module;
		}
	}

	return NULL;
}

/** Get the value of a pointer in a module.
 * @param module	Module to look in.
 * @param name		Name of pointer symbol. */
static void *module_lookup_pointer(module_t *module, const char *name) {
	symbol_t *sym;

	sym = symbol_table_lookup_name(&module->symtab, name, false, false);
	return (sym) ? (void *)(*(ptr_t *)sym->addr) : NULL;
}

/** Check module dependencies.
 * @param module	Module to check.
 * @param depbuf	Buffer to store name of missing dependency in.
 * @return		0 if dependencies satisfied, negative error code
 *			on failure. */
static int module_check_deps(module_t *module, char *depbuf) {
	module_t *dep;
	symbol_t *sym;
	int i;

	/* No dependencies symbol means no dependencies. */
	sym = symbol_table_lookup_name(&module->symtab, "__module_deps", false, false);
	if(sym == NULL) {
		module->deps = NULL;
		return 0;
	}

	module->deps = (const char **)sym->addr;

	/* Loop through each dependency. */
	for(i = 0; module->deps[i] != NULL; i++) {
		if(strnlen(module->deps[i], MODULE_NAME_MAX) == MODULE_NAME_MAX) {
			/* Meh, ignore it. */
			continue;
		} else if(strcmp(module->deps[i], module->name) == 0) {
			kprintf(LOG_DEBUG, "module: module %p(%s) depends on itself\n", module, module->name);
			return -ERR_FORMAT_INVAL;
		}

		dep = module_find(module->deps[i]);
		if(dep == NULL) {
			/* Unloaded dependency, store it in depbuf and return
			 * error. */
			if(depbuf != NULL) {
				strncpy(depbuf, module->deps[i], MODULE_NAME_MAX);
			}
			return -ERR_DEP_MISSING;
		}
	}

	/* All dependencies are loaded, go through and reference them. We do
	 * this here rather than in the previous loop so that if we have to
	 * return error we don't have to go and remove the reference on
	 * everything we've checked already. */
	for(i = 0; module->deps[i] != NULL; i++) {
		refcount_inc(&(module_find(module->deps[i]))->count);
	}

	return 0;
}

/** Load a kernel module.
 *
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided, which should be MODULE_NAME_MAX bytes long.
 * The intended usage of this function is to keep on calling it and loading
 * each unmet dependency it specifies until it succeeds.
 *
 * @param path		Path to module on filesystem.
 * @param depbuf	Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX bytes long).
 *
 * @return		0 on success, negative error code on failure. If a
 *			required dependency is not loaded, the ERR_DEP_MISSING
 *			error code is returned.
 */
int module_load(const char *path, char *depbuf) {
	vfs_node_t *node;
	module_t *module;
	int ret;

	if(!path || !depbuf) {
		return -ERR_PARAM_INVAL;
	}

	/* Look up the node and check it is the correct type. */
	if((ret = vfs_node_lookup(path, true, VFS_NODE_FILE, &node)) != 0) {
		return ret;
	} else if(!elf_module_check(node)) {
		vfs_node_release(node);
		return -ERR_TYPE_INVAL;
	}

	/* Create a module structure for the module. */
	module = kmalloc(sizeof(module_t), MM_SLEEP);
	list_init(&module->header);
	refcount_set(&module->count, 0);
	symbol_table_init(&module->symtab);
	module->node = node;
	module->shdrs = NULL;
	module->load_base = NULL;
	module->load_size = 0;

	/* Take the module lock in order to serialize module loading. */
	mutex_lock(&module_lock, 0);

	/* Perform first stage of loading the module. */
	if((ret = elf_module_load(module)) != 0) {
		goto fail;
	}

	/* Retrieve the module information. */
	module->name = module_lookup_pointer(module, "__module_name");
	module->description = module_lookup_pointer(module, "__module_desc");
	module->init = module_lookup_pointer(module, "__module_init");
	module->unload = module_lookup_pointer(module, "__module_unload");
	if(!module->name || !module->description || !module->init) {
		kprintf(LOG_DEBUG, "module: information for module %p is invalid\n", module);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(strnlen(module->name, MODULE_NAME_MAX) == MODULE_NAME_MAX) {
		kprintf(LOG_DEBUG, "module: name of module %p is too long\n", module);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Check if a module with this name already exists. */
	if(module_find(module->name) != NULL) {
		ret = -ERR_ALREADY_EXISTS;
		goto fail;
	}

	/* Check whether the module's dependencies are loaded. */
	if((ret = module_check_deps(module, depbuf)) != 0) {
		goto fail;
	}

	/* Perform external relocations on the module. At this point all
	 * dependencies are loaded, so assuming the module's dependencies are
	 * correct, external symbol lookups can be done. */
	if((ret = elf_module_relocate(module, true)) != 0) {
		goto fail;
	}

	/* Add the module to the modules list. Do this before calling the
	 * initialisation function so backtraces will have the correct symbols
	 * if the call ends up inside KDBG. */
	list_append(&module_list, &module->header);

	/* Call the module initialisation function. */
	dprintf("module: calling init function %p for module %p(%s)...\n",
		module->init, module, module->name);
	ret = module->init();
	if(ret != 0) {
		list_remove(&module->header);
		/* FIXME: Leaves a reference on the module's dependencies. */
		goto fail;
	}

	/* Node no longer required. */
	vfs_node_release(module->node);
	module->node = NULL;

	kprintf(LOG_DEBUG, "module: successfully loaded module %p(%s)\n", module, module->name);
	mutex_unlock(&module_lock);
	return 0;
fail:
	if(module->load_base) {
		module_mem_free(module->load_base, module->load_size);
	}
	if(module->shdrs) {
		kfree(module->shdrs);
	}
	symbol_table_destroy(&module->symtab);
	vfs_node_release(module->node);
	kfree(module);

	mutex_unlock(&module_lock);
	return ret;
}

/** Look up symbol from address.
 *
 * Looks for the symbol corresponding to an address in all module symbol
 * tables, and gets the offset of the address in the symbol.
 *
 * @param addr		Address to lookup.
 * @param offp		Where to store symbol offset (can be NULL).
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *module_symbol_lookup_addr(ptr_t addr, size_t *offp) {
	module_t *module;
	symbol_t *sym;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		sym = symbol_table_lookup_addr(&module->symtab, addr, offp);
		if(sym) {
			return sym;
		}
	}

	return NULL;
}

/** Look up symbol from name.
 *
 * Looks for a symbol with the name specified in all module symbol tables. If
 * specified, will only look for global and/or exported symbols.
 *
 * @param name		Name to lookup.
 * @param global	Whether to only look up global symbols.
 * @param exported	Whether to only look up exported symbols.
 *
 * @return		Pointer to the symbol structure, or NULL if not found.
 */
symbol_t *module_symbol_lookup_name(const char *name, bool global, bool exported) {
	module_t *module;
	symbol_t *sym;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		sym = symbol_table_lookup_name(&module->symtab, name, global, exported);
		if(sym) {
			return sym;
		}
	}

	return NULL;
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

	kprintf(LOG_NONE, "Name             Count Size     Description\n");
	kprintf(LOG_NONE, "====             ===== ====     ===========\n");

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		kprintf(LOG_NONE, "%-16s %-5d %-8zu %s\n", module->name,
		        refcount_get(&module->count), module->load_size,
		        module->description);
	}

	return KDBG_OK;
}

#if 0
# pragma mark System calls.
#endif

/** Load a kernel module.
 *
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided, which should be MODULE_NAME_MAX bytes long.
 * The intended usage of this function is to keep on calling it and loading
 * each unmet dependency it specifies until it succeeds.
 *
 * @param path		Path to module on filesystem.
 * @param depbuf	Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX bytes long).
 *
 * @return		0 on success, negative error code on failure. If a
 *			required dependency is not loaded, the ERR_DEP_MISSING
 *			error code is returned.
 */
int sys_module_load(const char *path, char *depbuf) {
	char *kpath = NULL, kdepbuf[MODULE_NAME_MAX];
	int ret, err;

	/* Copy the path across. */
	if((ret = strndup_from_user(path, PATH_MAX, MM_SLEEP, &kpath)) != 0) {
		return (handle_t)ret;
	}

	ret = module_load(kpath, kdepbuf);
	if(ret == -ERR_DEP_MISSING) {
		if((err = memcpy_to_user(depbuf, kdepbuf, MODULE_NAME_MAX)) != 0) {
			ret = err;
		}
	}

	return ret;
}
