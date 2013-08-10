/*
 * Copyright (C) 2009-2013 Alex Smith
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
#include <mm/phys.h>
#include <mm/safe.h>

#include <sync/mutex.h>

#include <assert.h>
#include <kdb.h>
#include <kernel.h>
#include <kboot.h>
#include <module.h>
#include <status.h>

/** Define to enable debug output from the module loader. */
//#define DEBUG_MODULE

#ifdef DEBUG_MODULE
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Structure describing a boot module. */
typedef struct boot_module {
	list_t header;			/**< Link to modules list. */
	void *mapping;			/**< Pointer to mapped module data. */
	size_t size;			/**< Size of the module data. */
	object_handle_t *handle;	/**< File handle for the module data. */
	char *name;			/**< Name of the module. */
} boot_module_t;

/** List of loaded modules. */
static LIST_DECLARE(module_list);
static MUTEX_DECLARE(module_lock, 0);

#ifdef KERNEL_MODULE_BASE
/** Module memory allocation space. */
static ptr_t next_module_addr = KERNEL_MODULE_BASE;
static size_t remaining_module_size = KERNEL_MODULE_SIZE;
#endif

/** Kernel module structure. */
module_t kernel_module;

/** Allocate memory suitable to hold a kernel module.
 * @param size		Size of the allocation.
 * @return		Address allocated or 0 if no available memory. */
ptr_t module_mem_alloc(size_t size) {
	#ifdef KERNEL_MODULE_BASE
		page_t *page;
		ptr_t addr;
		size_t i;

		size = ROUND_UP(size, PAGE_SIZE);

		if(size > remaining_module_size)
			return 0;

		addr = next_module_addr;

		mmu_context_lock(&kernel_mmu_context);
		for(i = 0; i < size; i += PAGE_SIZE) {
			page = page_alloc(MM_BOOT);
			mmu_context_map(&kernel_mmu_context, addr + i, page->addr,
				VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE,
				MM_BOOT);
		}
		mmu_context_unlock(&kernel_mmu_context);

		next_module_addr += size;
		remaining_module_size -= size;
		return addr;
	#else
		return kmem_alloc(ROUND_UP(size, PAGE_SIZE), MM_NOWAIT);
	#endif
}

/** Free memory holding a module.
 * @param base		Base of the allocation.
 * @param size		Size of the allocation. */
void module_mem_free(ptr_t base, size_t size) {
	#ifndef KERNEL_MODULE_BASE
	kmem_free((void *)base, ROUND_UP(size, PAGE_SIZE));
	#endif
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

/** Find and check a module's information.
 * @param module	Module being loaded.
 * @return		Status code describing result of the operation. */
static status_t find_module_info(module_t *module) {
	symbol_t sym;
	bool found;

	/* Retrieve the module information. */
	found = elf_symbol_lookup(&module->image, "__module_name", false, false, &sym);
	module->name = (found) ? (void *)sym.addr : NULL;
	found = elf_symbol_lookup(&module->image, "__module_desc", false, false, &sym);
	module->description = (found) ? (void *)sym.addr : NULL;
	found = elf_symbol_lookup(&module->image, "__module_init", false, false, &sym);
	module->init = (found) ? (void *)(*(ptr_t *)sym.addr) : NULL;
	found = elf_symbol_lookup(&module->image, "__module_unload", false, false, &sym);
	module->unload = (found) ? (void *)(*(ptr_t *)sym.addr) : NULL;

	/* Check if it is valid. */
	if(!module->name || !module->description || !module->init) {
		kprintf(LOG_NOTICE, "module: information for module %s is invalid\n",
			module->image.name);
		return STATUS_MALFORMED_IMAGE;
	} else if(strnlen(module->name, MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
		kprintf(LOG_NOTICE, "module: name of module %s is too long\n",
			module->image.name);
		return STATUS_MALFORMED_IMAGE;
	} else if(strnlen(module->description, MODULE_DESC_MAX + 1) == (MODULE_DESC_MAX + 1)) {
		kprintf(LOG_NOTICE, "module: description of module %s is too long\n",
			module->image.name);
		return STATUS_MALFORMED_IMAGE;
	}

	/* Check if a module with this name already exists. */
	if(module_find(module->name))
		return STATUS_ALREADY_EXISTS;

	return STATUS_SUCCESS;
}

/** Finish loading a module.
 * @param module	Module being loaded.
 * @param namep		Where to store name of unmet dependency.
 * @param depp		If a dependency is loaded but not ready, a pointer to
 *			it will be stored here.
 * @return		Status code describing result of the operation. */
static status_t finish_module(module_t *module, const char **namep, module_t **depp) {
	symbol_t sym;
	size_t i;
	module_t *dep;
	status_t ret;

	module->state = MODULE_DEPS;

	/* No dependencies symbol means no dependencies. */
	if(elf_symbol_lookup(&module->image, "__module_deps", false, false, &sym)) {
		module->deps = (const char **)sym.addr;
	} else {
		module->deps = NULL;
	}

	/* Loop through each dependency. The array is NULL-terminated. */
	for(i = 0; module->deps && module->deps[i]; i++) {
		if(strnlen(module->deps[i], MODULE_NAME_MAX + 1) == (MODULE_NAME_MAX + 1)) {
			kprintf(LOG_WARN, "module: module %s has invalid dependency\n",
				module->name);
			return STATUS_MALFORMED_IMAGE;
		}

		dep = module_find(module->deps[i]);
		if(!dep || dep->state != MODULE_READY) {
			if(namep)
				*namep = module->deps[i];
			if(depp)
				*depp = dep;
			return STATUS_MISSING_LIBRARY;
		}
	}

	/* Perform remaining relocations on the module. At this point all
	 * dependencies are loaded, so assuming the module's dependencies are
	 * correct, external symbol lookups can be done. */
	ret = elf_module_finish(&module->image);
	if(ret != STATUS_SUCCESS)
		return ret;

	module->state = MODULE_INIT;

	/* Call the module initialization function. */
	dprintf("module: calling init function %p for module %p (%s)...\n",
		module->init, module, module->name);
	ret = module->init();
	if(ret != STATUS_SUCCESS)
		return ret;

	/* Reference all the dependencies. We leave this until now to avoid
	 * having to go through and remove the reference if anything above
	 * fails. */
	for(i = 0; module->deps && module->deps[i] != NULL; i++) {
		dep = module_find(module->deps[i]);
		refcount_inc(&dep->count);
	}

	module->state = MODULE_READY;

	kprintf(LOG_NOTICE, "module: successfully loaded module %s (%s)\n",
		module->name, module->description);
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
 * @param path		Path to module on filesystem.
 * @param depbuf	Where to store name of unmet dependency (should be
 *			MODULE_NAME_MAX + 1 bytes long).
 *
 * @return		Status code describing result of the operation. If a
 *			required dependency is not loaded, the function will
 *			return STATUS_MISSING_LIBRARY.
 */
status_t module_load(const char *path, char *depbuf) {
	object_handle_t *handle;
	module_t *module;
	const char *dep;
	status_t ret;

	assert(path);

	/* Open a handle to the file. */
	ret = fs_open(path, FILE_RIGHT_READ, 0, 0, &handle);
	if(ret != STATUS_SUCCESS)
		return ret;

	module = kmalloc(sizeof(module_t), MM_KERNEL);
	list_init(&module->header);
	refcount_set(&module->count, 0);

	/* Take the module lock to serialise module loading. */
	mutex_lock(&module_lock);

	/* Perform first stage of loading the module. */
	ret = elf_module_load(handle, path, &module->image);
	object_handle_release(handle);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&module_lock);
		kfree(module);
		return ret;
	}

	ret = find_module_info(module);
	if(ret != STATUS_SUCCESS) {
		elf_module_destroy(&module->image);
		mutex_unlock(&module_lock);
		kfree(module);
		return ret;
	}

	module->state = MODULE_LOADED;
	list_append(&module_list, &module->header);

	ret = finish_module(module, &dep, NULL);
	if(ret != STATUS_SUCCESS) {
		if(ret == STATUS_MISSING_LIBRARY && depbuf)
			strncpy(depbuf, dep, MODULE_NAME_MAX + 1);

		list_remove(&module->header);
		elf_module_destroy(&module->image);
		mutex_unlock(&module_lock);
		kfree(module);
		return ret;
	}

	mutex_unlock(&module_lock);
	return STATUS_SUCCESS;
}

/**
 * Look up a kernel symbol by address.
 *
 * Looks for the kernel symbol corresponding to an address in all loaded
 * modules, and gets the offset of the address in the symbol. Note that
 * symbol lookups should only be performed in KDB or by the module loader:
 * they do not take a lock and are therefore unsafe.
 *
 * @param addr		Address to lookup.
 * @param symbol	Symbol structure to fill in.
 * @param offp		Where to store symbol offset (can be NULL).
 *
 * @return		Whether a symbol was found for the address. If a symbol
 *			is not found, the symbol structure is filled with
 *			dummy information (name set to "<unknown>", everything
 *			else set to 0).
 */
bool symbol_from_addr(ptr_t addr, symbol_t *symbol, size_t *offp) {
	module_t *module;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		if(elf_symbol_from_addr(&module->image, addr, symbol, offp))
			return true;
	}

	symbol->addr = symbol->size = symbol->global = symbol->exported = 0;
	symbol->name = "<unknown>";
	if(offp)
		*offp = 0;

	return false;
}

/**
 * Look up a kernel symbol by name.
 *
 * Looks for a symbol with the specified name in all loaded modules. If
 * requested, only global and/or exported symbols will be returned. Note that
 * symbol lookups should only be performed in KDB or by the module loader:
 * they do not take a lock and are therefore unsafe.
 *
 * @param name		Name to lookup.
 * @param global	Whether to only look up global symbols.
 * @param exported	Whether to only look up exported symbols.
 * @param symbol	Symbol structure to fill in.
 *
 * @return		Whether a symbol by this name was found. If the symbol
 *			is not found, the symbol structure is filled with
 *			dummy information (name set to "<unknown>", everything
 *			else set to 0).
 */
bool symbol_lookup(const char *name, bool global, bool exported, symbol_t *symbol) {
	module_t *module;

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		if(elf_symbol_lookup(&module->image, name, global, exported, symbol))
			return true;
	}

	symbol->addr = symbol->size = symbol->global = symbol->exported = 0;
	symbol->name = "<unknown>";
	return false;
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

	kdb_printf("Name             State  Count Image Description\n");
	kdb_printf("====             =====  ===== ===== ===========\n");

	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		kdb_printf("%-16s ", module->name);

		switch(module->state) {
		case MODULE_LOADED:
			kdb_printf("Loaded ");
			break;
		case MODULE_DEPS:
			kdb_printf("Deps   ");
			break;
		case MODULE_INIT:
			kdb_printf("Init   ");
			break;
		case MODULE_READY:
			kdb_printf("Ready  ");
			break;
		case MODULE_UNLOAD:
			kdb_printf("Unload ");
			break;
		}

		kdb_printf("%-5d %-5d %s\n", refcount_get(&module->count),
			module->image.id, module->description);
	}

	return KDB_SUCCESS;
}

/** Initialize the module system.. */
__init_text void module_early_init(void) {
	/* Initialize the kernel module structure. */
	list_init(&kernel_module.header);
	refcount_set(&kernel_module.count, 1);
	kernel_module.name = "kernel";
	kernel_module.description = "Kiwi kernel";
	kernel_module.state = MODULE_READY;
	elf_init(&kernel_module.image);
	list_append(&module_list, &kernel_module.header);

	/* Register the KDB command. */
	kdb_register_command("modules", "Display information about loaded kernel modules.",
		kdb_cmd_modules);
}

/** Finish loading a boot module.
 * @param module	Module to finish. */
static __init_text void finish_boot_module(module_t *module) {
	const char *name;
	module_t *dep;
	status_t ret;

	while(true) {
		ret = finish_module(module, &name, &dep);
		if(ret == STATUS_MISSING_LIBRARY) {
			if(!dep) {
				fatal("Boot module %s depends on %s which is not available",
					module->name, name);
			} else if(dep->state != MODULE_LOADED) {
				fatal("Circular module dependency detected for %s",
					dep->name);
			}

			finish_boot_module(dep);
		} else if(ret != STATUS_SUCCESS) {
			fatal("Failed to load boot module %s (%d)", module->name, ret);
		} else {
			break;
		}
	}
}

/** Load boot kernel modules. */
__init_text void module_init(void) {
	const char *name;
	void *mapping;
	object_handle_t *handle;
	module_t *module;
	status_t ret;

	/* Perform the first stage of loading all the modules to find out their
	 * names and dependencies. */
	KBOOT_ITERATE(KBOOT_TAG_MODULE, kboot_tag_module_t, tag) {
		name = kboot_tag_data(tag, 0);
		mapping = phys_map(tag->addr, tag->size, MM_BOOT);
		handle = file_from_memory(mapping, tag->size);

		module = kmalloc(sizeof(module_t), MM_BOOT);
		list_init(&module->header);
		refcount_set(&module->count, 0);

		ret = elf_module_load(handle, name, &module->image);
		object_handle_release(handle);
		phys_unmap(mapping, tag->size, true);
		if(ret != STATUS_SUCCESS) {
			if(ret == STATUS_UNKNOWN_IMAGE) {
				/* Assume that it is a filesystem image rather
				 * than a module. */
				kfree(module);
				continue;
			}

			fatal("Failed to load boot module %s (%d)", name, ret);
		}

		ret = find_module_info(module);
		if(ret != STATUS_SUCCESS)
			fatal("Boot module %s has invalid information", name);

		module->state = MODULE_LOADED;
		list_append(&module_list, &module->header);
	}

	/* Now all of the modules are partially loaded, we can resolve
	 * dependencies and load them all in the correct order. */
	LIST_FOREACH(&module_list, iter) {
		module = list_entry(iter, module_t, header);

		/* May already be loaded due to a dependency from another
		 * module. */
		if(module->state == MODULE_READY)
			continue;

		finish_boot_module(module);
	}
}

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
	status_t ret, err;

	/* Copy the path across. */
	ret = strndup_from_user(path, FS_PATH_MAX, &kpath);
	if(ret != STATUS_SUCCESS)
		return ret;

	ret = module_load(kpath, kdepbuf);
	if(ret == STATUS_MISSING_LIBRARY && depbuf) {
		err = memcpy_to_user(depbuf, kdepbuf, MODULE_NAME_MAX + 1);
		if(err != STATUS_SUCCESS)
			ret = err;
	}

	kfree(kpath);
	return ret;
}

#if 0
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
#endif
