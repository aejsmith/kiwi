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

#include <mm/malloc.h>

#include <sync/mutex.h>

#include <errors.h>
#include <kdbg.h>
#include <module.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Architecture memory allocation functions. */
extern void *module_mem_alloc(size_t size, int mmflag);
extern void module_mem_free(void *base, size_t size);

/** Architecture ELF loading functions. */
extern int module_elf_relocate(module_t *module, void *image, size_t size, bool external);

/** List of loaded modules. */
static LIST_DECLARE(module_list);

/** Lock to serialize module loading. */
static MUTEX_DECLARE(module_lock);

/*
 * Internal ELF loader.
 */

/** Get a section header from a module structure. */
#define GET_SECT(m, i)	((elf_shdr_t *)(((ptr_t)((m)->shdrs)) + (m)->ehdr.e_shentsize * (i)))

#if 0
/** Get value of a symbol from a symbol number in a module.
 * @param module	Module to get value from.
 * @param num		Number of the symbol.
 * @param external	Whether to handle external or internal symbols.
 * @param valp		Where to store symbol value.
 * @return		1 on success, 0 if lookup not done, negative error
 *			code on failure. */
static int module_elf_get_sym(elf_module_t *module, size_t num, bool external, ElfW(Addr) *valp) {
	ElfW(Shdr) *symtab;
	ElfW(Sym) *sym;
	const char *strtab;
	ksym_t *ksym;

	symtab = elf_module_find_section(module, ".symtab");
	if(symtab == NULL) {
		return -ENOEXEC;
	}

	strtab = (const char *)GET_SECT(module, symtab->sh_link)->sh_addr;

	if(num >= symtab->sh_size / symtab->sh_entsize) {
		return -EINVAL;
	}

	sym = (ElfW(Sym) *)(symtab->sh_addr + (symtab->sh_entsize * num));
	if(sym->st_shndx == 0) {
		if(!external) {
			return 0;
		}

		/* External symbol, look up in the kernel symbol table. */
		ksym = ksym_lookup_name(strtab + sym->st_name, true, true);
		if(ksym == NULL) {
			dprintf("elf: module symbol '%s' undefined\n", strtab + sym->st_name);
			return -ENOEXEC;
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
#endif

/** Find a section in an ELF module.
 * @param module	Module to find in.
 * @param name		Name of section to find.
 * @return		Pointer to section or NULL if not found. */
static elf_shdr_t *module_elf_find_section(module_t *module, const char *name) {
	const char *strtab;
	elf_shdr_t *sect;
	size_t i;

	strtab = (const char *)GET_SECT(module, module->ehdr.e_shstrndx)->sh_addr;
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = GET_SECT(module, i);
		if(strcmp(strtab + sect->sh_name, name) == 0) {
			return sect;
		}
	}

	return NULL;
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
		sect = GET_SECT(module, i);

		if(sect->sh_type == ELF_SHT_PROGBITS || sect->sh_type == ELF_SHT_NOBITS ||
		   sect->sh_type == ELF_SHT_STRTAB || sect->sh_type == ELF_SHT_SYMTAB) {
			module->load_size += sect->sh_size;
		}
	}

	if(module->load_size == 0) {
		dprintf("module: no loadable sections in module 0x%p\n", module);
		return -ERR_BAD_EXEC;
	}

	/* Allocate space to load the module into. */
	module->load_base = dest = module_mem_alloc(module->load_size, MM_SLEEP);

	/* Now for each section copy the data into the allocated area. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = GET_SECT(module, i);

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
					return -ERR_BAD_EXEC;
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
		return -ERR_BAD_EXEC;
	}

	/* Iterate over each symbol in the section. */
	strtab = (const char *)GET_SECT(module, symtab->sh_link)->sh_addr;
	for(i = 0; i < symtab->sh_size / symtab->sh_entsize; i++) {
		sym = (elf_sym_t *)(symtab->sh_addr + (symtab->sh_entsize * i));
		if(sym->st_shndx == 0 || sym->st_shndx > module->ehdr.e_shnum) {
			continue;
		}

		/* Get the section that the symbol corresponds to. */
		sect = GET_SECT(module, sym->st_shndx);
		if((sect->sh_flags & ELF_SHF_ALLOC) == 0) {
			continue;
		}

		/* Fix up the symbol address. */
		sym->st_value += sect->sh_addr;

		/* Only need to store certain types of symbol, and ignore
		 * module export symbols. */
		if(ELF_ST_TYPE(sym->st_info) == ELF_STT_SECTION || ELF_ST_TYPE(sym->st_info) == ELF_STT_FILE) {
			continue;
		} else if(strncmp(strtab + sym->st_name, "__modexport_", 12) == 0) {
			continue;
		}

		/* Don't mark as exported yet, we handle exports later. */
		symtab_insert(&module->symtab, strtab + sym->st_name, sym->st_value,
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
	if((module->ehdr.e_shoff + size) > sh_size) {
		return -ERR_BAD_EXEC;
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
	ret = module_elf_relocate(module, image, size, false);
	if(ret != 0) {
		return ret;
	}

	/* Find the module exports section and mark any exported symbols
	 * as exported. */
	exports = module_elf_find_section(module, ".modexports");
	if(exports != NULL) {
		for(i = 0; i < exports->sh_size; i += sizeof(const char *)) {
			export = (const char *)(*(ptr_t *)(exports->sh_addr + i));
			if((ptr_t)export < (ptr_t)image || (ptr_t)export >= ((ptr_t)image + size)) {
				dprintf("module: export string in module 0x%p is invalid\n", module);
				return -ERR_BAD_EXEC;
			}

			/* Find the symbol and mark it as exported. */
			sym = symtab_lookup_name(&module->symtab, export, true, false);
			if(sym == NULL) {
				dprintf("elf: exported symbol %s in module 0x%p cannot be found\n",
				        export, module);
				return -ERR_BAD_EXEC;
			}

			sym->exported = true;

			dprintf("elf: exported symbol %s in module 0x%p\n", export, module);
		}
	}

	return 0;
}

/*
 * Public functions.
 */

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
int module_load(void *image, size_t size, char *dep) {
	module_t *module;
	int ret;

	if(!module_check(image, size)) {
		return -ERR_BAD_EXEC;
	}

	/* Create a module structure for the module. */
	module = kmalloc(sizeof(module_t), MM_SLEEP);
	list_init(&module->header);
	refcount_set(&module->count, 0);
	symtab_init(&module->symtab);
	module->shdrs = NULL;
	module->load_base = NULL;
	module->load_size = 0;

	/* Perform first stage of loading the module. */
	ret = module_elf_load(module, image, size);
	if(ret != 0) {
		goto fail;
	}

	return 0;
fail:
	if(module->load_base) {
		module_mem_free(module->load_base, module->load_size);
	}
	if(module->shdrs) {
		kfree(module->shdrs);
	}
	kfree(module);
	return ret;
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
