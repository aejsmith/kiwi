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

/** Architecture ELF loading functions. */
extern int module_elf_relocate(module_t *module, void *image, size_t size, bool external,
                               int (*get_sym)(module_t *, size_t, bool, elf_addr_t *));

/** List of loaded modules. */
static LIST_DECLARE(module_list);

/** Lock to serialize module loading. */
static MUTEX_DECLARE(module_lock);

/*
 * Module memory allocation functions.
 */

#ifdef KERNEL_MODULE_BASE
/** Arenas used for allocating memory for kernel modules. */
static vmem_t *module_raw_arena;
static vmem_t *module_arena;

/** Initialize the kernel module allocation arena. */
static void module_mem_init(void) {
	/* This architecture requires a specific range for module allocations,
	 * create our own arenas. */
	module_raw_arena = vmem_create("module_raw_arena", KERNEL_MODULE_BASE,
	                               KERNEL_MODULE_SIZE, PAGE_SIZE, NULL,
	                               NULL, NULL, 0, MM_FATAL);
	module_arena = vmem_create("module_arena", NULL, 0, PAGE_SIZE,
	                           kheap_anon_afunc, kheap_anon_ffunc,
	                           module_raw_arena, 0, MM_FATAL);
}
#else
/** Arena used for allocating memory for kernel modules. */
static vmem_t *module_arena;

/** Initialize the kernel module allocation arena. */
static void module_mem_init(void) {
	/* This architecture has no specific location requirements for modules,
	 * have the module arena inherit from the kernel heap. */
	module_arena = vmem_create("module_arena", NULL, 0, PAGE_SIZE,
	                           kheap_anon_afunc, kheap_anon_ffunc,
	                           &kheap_va_arena, 0, MM_FATAL);
}
#endif

/** Allocate memory suitable to hold a kernel module.
 * @param size		Size of the allocation.
 * @param mmflag	Allocation flags.
 * @return		Address allocated or NULL if no available memory. */
static void *module_mem_alloc(size_t size, int mmflag) {
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

/*
 * Internal ELF loader.
 */

/** Find a section in an ELF module.
 * @param module	Module to find in.
 * @param name		Name of section to find.
 * @return		Pointer to section or NULL if not found. */
static elf_shdr_t *module_elf_find_section(module_t *module, const char *name) {
	const char *strtab;
	elf_shdr_t *sect;
	size_t i;

	strtab = (const char *)MODULE_ELF_SECT(module, module->ehdr.e_shstrndx)->sh_addr;
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);
		if(strcmp(strtab + sect->sh_name, name) == 0) {
			return sect;
		}
	}

	return NULL;
}

/** Get value of a symbol from a symbol number in a module.
 * @param module	Module to get value from.
 * @param num		Number of the symbol.
 * @param external	Whether to handle external or internal symbols.
 * @param valp		Where to store symbol value.
 * @return		1 on success, 0 if lookup not done, negative error
 *			code on failure. */
static int module_elf_get_sym(module_t *module, size_t num, bool external, elf_addr_t *valp) {
	const char *strtab;
	elf_shdr_t *symtab;
	elf_sym_t *sym;
	symbol_t *ksym;

	symtab = module_elf_find_section(module, ".symtab");
	if(symtab == NULL) {
		return -ERR_OBJ_FORMAT_BAD;
	} else if(num >= (symtab->sh_size / symtab->sh_entsize)) {
		return -ERR_OBJ_FORMAT_BAD;
	}

	strtab = (const char *)MODULE_ELF_SECT(module, symtab->sh_link)->sh_addr;
	sym = (elf_sym_t *)(symtab->sh_addr + (symtab->sh_entsize * num));
	if(sym->st_shndx == ELF_SHN_UNDEF) {
		if(!external) {
			return 0;
		}

		/* External symbol, look up in the kernel and other modules. */
		ksym = symbol_lookup_name(strtab + sym->st_name, true, true);
		if(ksym == NULL) {
			dprintf("elf: module references undefined symbol: %s\n", strtab + sym->st_name);
			return -ERR_OBJ_FORMAT_BAD;
		}

		*valp = ksym->addr;
	} else {
		if(external) {
			return 0;
		}

		/* Internal symbol. */
		*valp = sym->st_value;
	}

	return 1;
}

/** Allocate memory for and load all sections.
 * @param module	Module structure.
 * @param image		Pointer to module image.
 * @param size		Size of the module image.
 * @return		0 on success, negative error code on failure. */
static int module_elf_load_sections(module_t *module, void *image, size_t size) {
	elf_shdr_t *sect;
	void *dest;
	size_t i;

	/* Calculate the total size. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);

		if(sect->sh_type == ELF_SHT_PROGBITS || sect->sh_type == ELF_SHT_NOBITS ||
		   sect->sh_type == ELF_SHT_STRTAB || sect->sh_type == ELF_SHT_SYMTAB) {
			module->load_size += sect->sh_size;
		}
	}

	if(module->load_size == 0) {
		dprintf("module: no loadable sections in module 0x%p\n", module);
		return -ERR_OBJ_FORMAT_BAD;
	}

	/* Allocate space to load the module into. */
	module->load_base = dest = module_mem_alloc(module->load_size, MM_SLEEP);

	/* Now for each section copy the data into the allocated area. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);

		if(sect->sh_type == ELF_SHT_PROGBITS || sect->sh_type == ELF_SHT_NOBITS ||
		   sect->sh_type == ELF_SHT_STRTAB || sect->sh_type == ELF_SHT_SYMTAB) {
			sect->sh_addr = (elf_addr_t)dest;

			dprintf("module: loading data for section %u to 0x%p (size: %u, type: %u)\n",
			        i, dest, sect->sh_size, sect->sh_type);

			if(sect->sh_type == ELF_SHT_NOBITS) {
				memset(dest, 0, sect->sh_size);
			} else {
				/* Check that the data is within the image. */
				if((sect->sh_offset + sect->sh_size) > size) {
					return -ERR_OBJ_FORMAT_BAD;
				}

				memcpy(dest, image + sect->sh_offset, sect->sh_size);
			}

			dest += sect->sh_size;
		}
	}

	return 0;
}

/** Fix and load symbols in an ELF module.
 * @param module	Module to add symbols for.
 * @return		0 on success, negative error code on failure. */
static int module_elf_load_symbols(module_t *module) {
	elf_shdr_t *symtab, *sect;
	const char *strtab;
	elf_sym_t *sym;
	size_t i;

	/* Try to find the symbol table section. */
	symtab = module_elf_find_section(module, ".symtab");
	if(symtab == NULL) {
		dprintf("module: module does not contain a symbol table\n");
		return -ERR_OBJ_FORMAT_BAD;
	}

	/* Iterate over each symbol in the section. */
	strtab = (const char *)MODULE_ELF_SECT(module, symtab->sh_link)->sh_addr;
	for(i = 0; i < symtab->sh_size / symtab->sh_entsize; i++) {
		sym = (elf_sym_t *)(symtab->sh_addr + (symtab->sh_entsize * i));
		if(sym->st_shndx == ELF_SHN_UNDEF || sym->st_shndx > module->ehdr.e_shnum) {
			continue;
		}

		/* Get the section that the symbol corresponds to. */
		sect = MODULE_ELF_SECT(module, sym->st_shndx);
		if((sect->sh_flags & ELF_SHF_ALLOC) == 0) {
			continue;
		}

		/* Fix up the symbol address. */
		sym->st_value += sect->sh_addr;

		/* Only need to store certain types of symbol, and ignore
		 * module export symbols. */
		if(ELF_ST_TYPE(sym->st_info) == ELF_STT_SECTION || ELF_ST_TYPE(sym->st_info) == ELF_STT_FILE) {
			continue;
		} else if(strncmp(strtab + sym->st_name, "__module_export_", 12) == 0) {
			continue;
		}

		/* Don't mark as exported yet, we handle exports later. */
		symbol_table_insert(&module->symtab, strtab + sym->st_name, sym->st_value,
		                    sym->st_size, (ELF_ST_BIND(sym->st_info)) ? true : false,
		                    false);

		dprintf("module: added symbol %s to module 0x%p (addr: 0x%p, size: 0x%p)\n",
			strtab + sym->st_name, module, sym->st_value, sym->st_size);
	}

	return 0;
}

/** Load an ELF kernel module.
 * @param module	Module structure for module.
 * @param image		Memory buffer containing module.
 * @param size		Size of memory buffer.
 * @return		0 on success, negative error code on failure. */
static int module_elf_load(module_t *module, void *image, size_t size) {
	elf_shdr_t *exports;
	const char *export;
	size_t sh_size, i;
	symbol_t *sym;
	int ret;

	/* The call to module_check() guarantees that there is at least an
	 * ELF header in the image. Make a copy of it. */
	memcpy(&module->ehdr, image, sizeof(elf_ehdr_t));

	/* Calculate the size of the section headers and ensure that we have
	 * the data available. */
	sh_size = module->ehdr.e_shnum * module->ehdr.e_shentsize;
	if((module->ehdr.e_shoff + sh_size) > size) {
		return -ERR_OBJ_FORMAT_BAD;
	}

	/* Make a copy of the section headers. */
	module->shdrs = kmemdup(image + module->ehdr.e_shoff, sh_size, MM_SLEEP);

	/* Load all loadable sections into memory. */
	ret = module_elf_load_sections(module, image, size);
	if(ret != 0) {
		return ret;
	}

	/* Fix symbol addresses and insert them into the symbol table. */
	ret = module_elf_load_symbols(module);
	if(ret != 0) {
		return ret;
	}

	/* Perform relocations. */
	ret = module_elf_relocate(module, image, size, false, module_elf_get_sym);
	if(ret != 0) {
		return ret;
	}

	/* Find the module exports section and mark any exported symbols
	 * as exported. */
	exports = module_elf_find_section(module, ".modexports");
	if(exports != NULL) {
		for(i = 0; i < exports->sh_size; i += sizeof(const char *)) {
			export = (const char *)(*(ptr_t *)(exports->sh_addr + i));

			/* Find the symbol and mark it as exported. */
			sym = symbol_table_lookup_name(&module->symtab, export, true, false);
			if(sym == NULL) {
				dprintf("module: exported symbol %s in module 0x%p cannot be found\n",
				        export, module);
				return -ERR_OBJ_FORMAT_BAD;
			}

			sym->exported = true;

			dprintf("module: exported symbol %s in module 0x%p\n", export, module);
		}
	}

	return 0;
}

/*
 * Main functions.
 */

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
			dprintf("module: module 0x%p(%s) depends on itself\n", module, module->name);
			return -ERR_OBJ_FORMAT_BAD;
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

/** Check whether a module is valid.
 *
 * Checks whether the supplied memory buffer points to a valid kernel module
 * image.
 *
 * @param image		Pointer to memory buffer.
 * @param size		Size of buffer.
 *
 * @return		True if module is valid, false if not.
 */
bool module_check(void *image, size_t size) {
	return elf_check(image, size, ELF_ET_REL);
}

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
int module_load(void *image, size_t size, char *depbuf) {
	module_t *module;
	int ret;

	if(!image || size == 0 || depbuf == NULL) {
		return -ERR_PARAM_INVAL;
	}

	/* Check if this is a valid module. */
	if(!module_check(image, size)) {
		return -ERR_OBJ_FORMAT_BAD;
	}

	/* Create a module structure for the module. */
	module = kmalloc(sizeof(module_t), MM_SLEEP);
	list_init(&module->header);
	refcount_set(&module->count, 0);
	symbol_table_init(&module->symtab);
	module->shdrs = NULL;
	module->load_base = NULL;
	module->load_size = 0;

	/* Take the module lock in order to serialize module loading. */
	mutex_lock(&module_lock, 0);

	/* Perform first stage of loading the module. */
	ret = module_elf_load(module, image, size);
	if(ret != 0) {
		goto fail;
	}

	/* Retrieve the module information. */
	module->name = module_lookup_pointer(module, "__module_name");
	module->description = module_lookup_pointer(module, "__module_desc");
	module->init = module_lookup_pointer(module, "__module_init");
	module->unload = module_lookup_pointer(module, "__module_unload");
	if(!module->name || !module->description || !module->init) {
		dprintf("module: information for module 0x%p is invalid\n", module);
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	} else if(strnlen(module->name, MODULE_NAME_MAX) == MODULE_NAME_MAX) {
		dprintf("module: name of module 0x%p is too long\n", module);
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	/* Check if a module with this name already exists. */
	if(module_find(module->name) != NULL) {
		ret = -ERR_OBJ_EXISTS;
		goto fail;
	}

	/* Check whether the module's dependencies are loaded. */
	ret = module_check_deps(module, depbuf);
	if(ret != 0) {
		goto fail;
	}

	/* Perform external relocations on the module. At this point all
	 * dependencies are loaded, so assuming the module's dependencies are
	 * correct, external symbol lookups can be done. */
	ret = module_elf_relocate(module, image, size, true, module_elf_get_sym);
	if(ret != 0) {
		goto fail;
	}

	/* Add the module to the modules list. Do this before calling the
	 * initialization function so backtraces will have the correct symbols
	 * if the call ends up inside KDBG. */
	list_append(&module_list, &module->header);

	/* Call the module initialization function. */
	dprintf("module: calling init function 0x%p for module 0x%p(%s)...\n",
		module->init, module, module->name);
	ret = module->init();
	if(ret != 0) {
		list_remove(&module->header);
		/* FIXME: Leaves a reference on the module's dependencies. */
		goto fail;
	}

	kprintf(LOG_DEBUG, "module: successfully loaded module 0x%p(%s)\n", module, module->name);
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

		kprintf(LOG_NONE, "%-16s %-5d %-8" PRIs " %s\n", module->name,
		        refcount_get(&module->count), module->load_size,
		        module->description);
	}

	return KDBG_OK;
}
