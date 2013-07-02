/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel module loader.
 */

#include <arch/memory.h>

#include <io/fs.h>

#include <lib/utility.h>

#include <mm/kmem.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/safe.h>

#include <security/cap.h>

#include <sync/mutex.h>

#include <kdb.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** List of loaded modules. */
static LIST_DECLARE(module_list);
static MUTEX_DECLARE(module_lock, 0);

#ifdef KERNEL_MODULE_BASE
/** Module memory allocation space. */
static ptr_t next_module_addr = KERNEL_MODULE_BASE;
static size_t remaining_module_size = KERNEL_MODULE_SIZE;
#endif

/** Allocate memory suitable to hold a kernel module.
 * @param size		Size of the allocation.
 * @return		Address allocated or NULL if no available memory. */
void *module_mem_alloc(size_t size) {
	#ifdef KERNEL_MODULE_BASE
	page_t *page;
	ptr_t addr;
	size_t i;

	size = ROUND_UP(size, PAGE_SIZE);

	if(size > remaining_module_size)
		return NULL;

	addr = next_module_addr;

	mmu_context_lock(&kernel_mmu_context);
	for(i = 0; i < size; i += PAGE_SIZE) {
		page = page_alloc(MM_BOOT);
		mmu_context_map(&kernel_mmu_context, addr + i, page->addr,
			MMU_MAP_WRITE | MMU_MAP_EXEC, MM_BOOT);
	}
	mmu_context_unlock(&kernel_mmu_context);

	next_module_addr += size;
	remaining_module_size -= size;
	return (void *)addr;
	#else
	return kmem_alloc(ROUND_UP(size, PAGE_SIZE), 0);
	#endif
}

/** Free memory holding a module.
 * @param base		Base of the allocation.
 * @param size		Size of the allocation. */
static void module_mem_free(void *base, size_t size) {
	#ifndef KERNEL_MODULE_BASE
	kmem_free(base, ROUND_UP(size, PAGE_SIZE));
	#endif
}

/** Allocate a module structure.
 * @param handle	Handle to the module data.
 * @return		Pointer to created structure. */
static module_t *module_alloc(object_handle_t *handle) {
	module_t *module;

	module = kmalloc(sizeof(module_t), MM_WAIT);
	list_init(&module->header);
	refcount_set(&module->count, 0);
	symbol_table_init(&module->symtab);
	module->handle = handle;
	module->shdrs = NULL;
	module->load_base = NULL;
	module->load_size = 0;
	return module;
}

/** Destroy a module.
 * @param module	Module to destroy. */
static void module_destroy(module_t *module) {
	symbol_table_destroy(&module->symtab);

	if(module->load_base)
		module_mem_free(module->load_base, module->load_size);

	if(module->shdrs)
		kfree(module->shdrs);

	kfree(module);
}

/** Find a module in the module list.
 * @param name		Name of module to find.
 * @return		Pointer to module structure, NULL if not found. */
static module_t *module_find(const char *name) {
	module_t *module;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);
		if(strcmp(module->name, name) == 0)
			return module;
	}

	return NULL;
}

/** Get the name of a kernel module.
 * @param handle	Handle to file containing module.
 * @param namebuf	Buffer to store name in (should be MODULE_NAME_MAX + 1
 *			bytes long).
 * @return		Status code describing result of the operation. */
status_t module_name(object_handle_t *handle, char *namebuf) {
	module_t *module;
	symbol_t *sym;
	status_t ret;

	if(!handle || !namebuf)
		return STATUS_INVALID_ARG;

	/* Take the module lock to serialise module loading. */
	mutex_lock(&module_lock);

	/* Perform first stage of loading the module. */
	module = module_alloc(handle);
	ret = elf_module_load(module);
	if(ret != STATUS_SUCCESS)
		goto out;

	/* Retrieve the name. */
	sym = symbol_table_lookup_name(&module->symtab, "__module_name", false, false);
	if(!sym) {
		ret = STATUS_MALFORMED_IMAGE;
		goto out;
	} else if(strnlen((char *)sym->addr, MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
		ret = STATUS_MALFORMED_IMAGE;
		goto out;
	}

	strcpy(namebuf, (char *)sym->addr);
out:
	module_destroy(module);
	mutex_unlock(&module_lock);
	return ret;
}

/** Check module dependencies.
 * @param module	Module to check.
 * @param depbuf	Buffer to store name of missing dependency in.
 * @return		STATUS_SUCCESS if dependencies satisfied,
 *			STATUS_MISSING_LIBRARY if not. */
static status_t module_check_deps(module_t *module, char *depbuf) {
	module_t *dep;
	symbol_t *sym;
	int i;

	/* No dependencies symbol means no dependencies. */
	sym = symbol_table_lookup_name(&module->symtab, "__module_deps", false, false);
	if(!sym) {
		module->deps = NULL;
		return STATUS_SUCCESS;
	}

	/* Loop through each dependency. */
	module->deps = (const char **)sym->addr;
	for(i = 0; module->deps[i] != NULL; i++) {
		if(strnlen(module->deps[i], MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
			/* Meh, ignore it. */
			continue;
		} else if(strcmp(module->deps[i], module->name) == 0) {
			kprintf(LOG_NOTICE, "module: module %s depends on itself\n", module, module->name);
			return STATUS_MALFORMED_IMAGE;
		}

		dep = module_find(module->deps[i]);
		if(!dep) {
			/* Unloaded dependency, store its name and return error. */
			if(depbuf != NULL)
				strncpy(depbuf, module->deps[i], MODULE_NAME_MAX + 1);

			return STATUS_MISSING_LIBRARY;
		}
	}

	/* All dependencies are loaded, go through and reference them. We do
	 * this here rather than in the previous loop so that if we have to
	 * return error we don't have to go and remove the reference on
	 * everything we've checked already. */
	for(i = 0; module->deps[i] != NULL; i++)
		refcount_inc(&(module_find(module->deps[i]))->count);

	return STATUS_SUCCESS;
}

/**
 * Load a kernel module.
 *
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided, which should be MODULE_NAME_MAX bytes long.
 * The intended usage of this function is to keep on calling it and loading
 * each unmet dependency it specifies until it succeeds.
 *
 * @param handle	Handle to module file.
 * @param depbuf	Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX + 1 bytes long).
 *
 * @return		Status code describing result of the operation. If a
 *			required dependency is not loaded, the function will
 *			return STATUS_MISSING_LIBRARY.
 */
status_t module_load(object_handle_t *handle, char *depbuf) {
	module_t *module;
	symbol_t *sym;
	status_t ret;

	if(!handle || !depbuf)
		return STATUS_INVALID_ARG;

	/* Check if the current process can load modules. */
	if(!cap_check(NULL, CAP_MODULE))
		return STATUS_PERM_DENIED;

	/* Take the module lock to serialise module loading. */
	mutex_lock(&module_lock);

	/* Perform first stage of loading the module. */
	module = module_alloc(handle);
	ret = elf_module_load(module);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Retrieve the module information. */
	sym = symbol_table_lookup_name(&module->symtab, "__module_name", false, false);
	module->name = (sym) ? (char *)sym->addr : NULL;
	sym = symbol_table_lookup_name(&module->symtab, "__module_desc", false, false);
	module->description = (sym) ? (char *)sym->addr : NULL;
	sym = symbol_table_lookup_name(&module->symtab, "__module_init", false, false);
	module->init = (sym) ? (void *)(*(ptr_t *)sym->addr) : NULL;
	sym = symbol_table_lookup_name(&module->symtab, "__module_unload", false, false);
	module->unload = (sym) ? (void *)(*(ptr_t *)sym->addr) : NULL;

	/* Check if it is valid. */
	if(!module->name || !module->description || !module->init) {
		kprintf(LOG_NOTICE, "module: information for module %p is invalid\n", module);
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	} else if(strnlen(module->name, MODULE_NAME_MAX) == MODULE_NAME_MAX) {
		kprintf(LOG_NOTICE, "module: name of module %p is too long\n", module);
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	} else if(strnlen(module->description, MODULE_DESC_MAX) == MODULE_DESC_MAX) {
		kprintf(LOG_NOTICE, "module: description of module %p is too long\n", module);
		ret = STATUS_MALFORMED_IMAGE;
		goto fail;
	}

	/* Check if a module with this name already exists. */
	if(module_find(module->name)) {
		ret = STATUS_ALREADY_EXISTS;
		goto fail;
	}

	/* Check whether the module's dependencies are loaded. */
	ret = module_check_deps(module, depbuf);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Perform remaining relocations on the module. At this point all
	 * dependencies are loaded, so assuming the module's dependencies are
	 * correct, external symbol lookups can be done. */
	ret = elf_module_finish(module);
	if(ret != STATUS_SUCCESS)
		goto fail;

	/* Publish the symbol table. Do this before calling the initialization
	 * function so backtraces will have the correct symbols if the call
	 * ends up inside KDB. */
	symbol_table_publish(&module->symtab);

	/* Call the module initialization function. */
	dprintf("module: calling init function %p for module %p(%s)...\n",
		module->init, module, module->name);
	ret = module->init();
	if(ret != STATUS_SUCCESS) {
		/* FIXME: Leaves a reference on the module's dependencies and
		 * leaves the symbols published. */
		goto fail;
	}

	list_append(&module_list, &module->header);
	module->handle = NULL;
	kprintf(LOG_NOTICE, "module: successfully loaded module %s (%s)\n",
		module->name, module->description);
	mutex_unlock(&module_lock);
	return STATUS_SUCCESS;
fail:
	module_destroy(module);
	mutex_unlock(&module_lock);
	return ret;
}

/** Print a list of loaded kernel modules.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_modules(int argc, char **argv, kdb_filter_t *filter) {
	module_t *module;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s\n\n", argv[0]);

		kdb_printf("Prints a list of all loaded kernel modules.\n");
		return KDB_SUCCESS;
	}

	kdb_printf("Name             Count Size     Description\n");
	kdb_printf("====             ===== ====     ===========\n");

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		kdb_printf("%-16s %-5d %-8zu %s\n", module->name,
			refcount_get(&module->count), module->load_size,
			module->description);
	}

	return KDB_SUCCESS;
}

/** Register the module KDB command. */
static __init_text void module_init(void) {
	kdb_register_command("modules", "Display information about loaded kernel modules.",
		kdb_cmd_modules);
}

INITCALL(module_init);

/**
 * Load a kernel module.
 *
 * Loads a kernel module from the filesystem. If any of the dependencies of the
 * module are not met, the name of the first unmet dependency encountered is
 * stored in the buffer provided. The intended usage of this function is to
 * keep on calling it and loading each unmet dependency it specifies until it
 * succeeds.
 *
 * @param path		Path to module on filesystem.
 * @param depbuf	Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX + 1 bytes long).
 *
 * @return		Status code describing result of the operation. If a
 *			required dependency is not loaded, the function will
 *			return STATUS_MISSING_LIBRARY.
 */
status_t kern_module_load(const char *path, char *depbuf) {
	char *kpath = NULL, kdepbuf[MODULE_NAME_MAX + 1];
	object_handle_t *handle;
	status_t ret, err;

	/* Copy the path across. */
	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Open a handle to the file. */
	ret = file_open(kpath, FILE_RIGHT_READ, 0, 0, NULL, &handle);
	if(ret != STATUS_SUCCESS) {
		kfree(kpath);
		return ret;
	}

	ret = module_load(handle, kdepbuf);
	if(ret == STATUS_MISSING_LIBRARY) {
		err = memcpy_to_user(depbuf, kdepbuf, MODULE_NAME_MAX + 1);
		if(err != STATUS_SUCCESS)
			ret = err;
	}

	object_handle_release(handle);
	kfree(kpath);
	return ret;
}

/** Get information on loaded kernel modules.
 * @param infop		Array of module information structures to fill in. If
 *			NULL, the function will only return the number of
 *			loaded modules.
 * @param countp	If infop is not NULL, this should point to a value
 *			containing the size of the provided array. Upon
 *			successful completion, the value will be updated to
 *			be the number of structures filled in. If infop is NULL,
 *			the number of loaded modules will be stored here.
 * @return		Status code describing result of the operation. */
status_t kern_module_info(module_info_t *infop, size_t *countp) {
	size_t i = 0, count = 0;
	module_info_t info;
	module_t *module;
	status_t ret;

	/* Check if the current process can load modules. */
	if(!cap_check(NULL, CAP_MODULE))
		return STATUS_PERM_DENIED;

	if(infop) {
		ret = memcpy_from_user(&count, countp, sizeof(count));
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(!count) {
			return STATUS_SUCCESS;
		}
	}

	mutex_lock(&module_lock);

	LIST_FOREACH(&module_list, iter) {
		if(infop) {
			module = list_entry(iter, module_t, header);

			strcpy(info.name, module->name);
			strcpy(info.desc, module->description);
			info.count = refcount_get(&module->count);
			info.load_size = module->load_size;

			ret = memcpy_to_user(&infop[i], &info, sizeof(info));
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&module_lock);
				return ret;
			}

			if(++i >= count)
				break;
		} else {
			i++;
		}
	}

	mutex_unlock(&module_lock);
	return memcpy_to_user(countp, &i, sizeof(i));
}
