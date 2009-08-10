/* Kiwi ELF binary loader
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
 * @brief		ELF binary loader.
 */

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/safe.h>

#include <proc/loader.h>

#include <elf.h>
#include <errors.h>
#include <fatal.h>
#include <init.h>
#include <module.h>

/** Check whether an FS node contains a valid ELF header.
 * @param node		Filesystem node.
 * @param type		Required ELF type.
 * @return		True if valid, false if not. */
static bool elf_check(vfs_node_t *node, int type) {
	elf_ehdr_t ehdr;
	size_t bytes;
	int ret;

	/* Read the ELF header in from the file. */
	if((ret = vfs_file_read(node, &ehdr, sizeof(elf_ehdr_t), 0, &bytes)) != 0) {
		return false;
	} else if(bytes != sizeof(elf_ehdr_t)) {
		return false;
	}

	/* Check the magic number and version. */
	if(strncmp((const char *)ehdr.e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
		return false;
	}

	/* Check whether it matches the architecture we're running on. */
	if(ehdr.e_ident[ELF_EI_CLASS] != ELF_CLASS ||
           ehdr.e_ident[ELF_EI_DATA] != ELF_ENDIAN ||
	   ehdr.e_machine != ELF_MACHINE) {
		return false;
	}

	/* Finally check type of binary. */
	return (ehdr.e_type == type);
}

#if 0
# pragma mark ELF executable loader.
#endif

#if CONFIG_LOADER_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void elf_binary_copy_data(elf_binary_t *data);

/** Handle an ELF_PT_LOAD program header.
 * @param data		ELF binary data structure.
 * @param i		Index of program header.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_phdr_load(elf_binary_t *data, size_t i) {
	int ret, flags = 0;
	ptr_t start, end;
	offset_t offset;
	size_t size;

	/* Work out the protection flags to use. */
	flags |= ((data->phdrs[i].p_flags & ELF_PF_R) ? VM_MAP_READ  : 0);
	flags |= ((data->phdrs[i].p_flags & ELF_PF_W) ? VM_MAP_WRITE : 0);
	flags |= ((data->phdrs[i].p_flags & ELF_PF_X) ? VM_MAP_EXEC  : 0);
	if(flags == 0) {
		dprintf("elf: program header %zu has no protection flags set\n", i);
		return -ERR_FORMAT_INVAL;
	}

	/* Set the private and fixed flags - we always want to insert at the
	 * position we say, and not share stuff. */
	flags |= VM_MAP_FIXED | VM_MAP_PRIVATE;

	/* Map the BSS if required. */
	if(data->phdrs[i].p_filesz != data->phdrs[i].p_memsz) {
		start = ROUND_DOWN(data->phdrs[i].p_vaddr + data->phdrs[i].p_filesz, PAGE_SIZE);
		end = ROUND_UP(data->phdrs[i].p_vaddr + data->phdrs[i].p_memsz, PAGE_SIZE);
		size = end - start;

		dprintf("elf: loading BSS for %zu to %p (size: %zu)\n", i, start, size);

		/* We have to have it writeable for us to be able to clear it
		 * later on. */
		if((flags & VM_MAP_WRITE) == 0) {
			dprintf("elf: program header %zu should be writeable\n", i);
			return -ERR_FORMAT_INVAL;
		}

		/* Create an anonymous memory region for it. */
		ret = vm_map_anon(data->binary->aspace, start, size, flags, NULL);
		if(ret != 0) {
			return ret;
		}
	}

	/* If file size is zero then this header is just uninitialized data. */
	if(data->phdrs[i].p_filesz == 0) {
		return 0;
	}

	/* Work out the address to map to and the offset in the file. */
	start = ROUND_DOWN(data->phdrs[i].p_vaddr, PAGE_SIZE);
	end = ROUND_UP(data->phdrs[i].p_vaddr + data->phdrs[i].p_filesz, PAGE_SIZE);
	size = end - start;
	offset = ROUND_DOWN(data->phdrs[i].p_offset, PAGE_SIZE);

	dprintf("elf: loading program header %zu to %p (size: %zu)\n", i, start, size);

	/* Map the data in. We do not need to check whether the supplied
	 * addresses are valid - aspace_map_file() will reject the call if they
	 * are. */
	return vm_map_file(data->binary->aspace, start, size, flags, data->binary->node, offset, NULL);
}

/** Check whether a binary is an ELF binary.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the binary is an ELF binary. */
static bool elf_binary_check(vfs_node_t *node) {
	return elf_check(node, ELF_ET_EXEC);
}

/** Load an ELF binary into an address space.
 * @param binary	Binary loader data structure.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_load(loader_binary_t *binary) {
	size_t bytes, size, i, load_count = 0;
	elf_binary_t *data;
	int ret;

	/* Allocate a structure to store data about the binary. */
	data = kmalloc(sizeof(elf_binary_t), MM_SLEEP);
	data->phdrs = NULL;
	data->binary = binary;

	/* Read in the ELF header. */
	if((ret = vfs_file_read(binary->node, &data->ehdr, sizeof(elf_ehdr_t), 0, &bytes)) != 0) {
		goto fail;
	} else if(bytes != sizeof(elf_ehdr_t)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Check that program headers are the right size... */
	if(data->ehdr.e_phentsize != sizeof(elf_phdr_t)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Allocate some memory for the program headers and load them too. */
	size = data->ehdr.e_phnum * data->ehdr.e_phentsize;
	data->phdrs = kmalloc(size, MM_SLEEP);
	ret = vfs_file_read(binary->node, data->phdrs, size, data->ehdr.e_phoff, &bytes);
	if(ret != 0) {
		goto fail;
	} else if(bytes != size) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Handle all the program headers. */
	for(i = 0; i < data->ehdr.e_phnum; i++) {
		switch(data->phdrs[i].p_type) {
		case ELF_PT_LOAD:
			ret = elf_binary_phdr_load(data, i);
			if(ret != 0) {
				goto fail;
			}
			load_count++;
			break;
		case ELF_PT_NOTE:
			/* These can be ignored without warning. */
			break;
		default:
			dprintf("elf: unknown program header type %u, ignoring\n", data->phdrs[i].p_type);
			break;
		}
	}

	/* Check if we actually loaded anything. */
	if(!load_count) {
		dprintf("elf: binary %p did not have any loadable program headers\n",
		        binary->node);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	binary->data = data;
	binary->entry = (ptr_t)data->ehdr.e_entry;
	return 0;
fail:
	if(data->phdrs) {
		kfree(data->phdrs);
	}
	kfree(data);

	return ret;
}

/** Finish binary loading, after address space is switched.
 * @param binary	Binary loader data structure.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_finish(loader_binary_t *binary) {
	elf_binary_t *data = (elf_binary_t *)binary->data;
	void *base;
	size_t i;
	int ret;

	for(i = 0; i < data->ehdr.e_phnum; i++) {
		switch(data->phdrs[i].p_type) {
		case ELF_PT_LOAD:
			if(data->phdrs[i].p_filesz >= data->phdrs[i].p_memsz) {
				break;
			}

			base = (void *)(data->phdrs[i].p_vaddr + data->phdrs[i].p_filesz);
			dprintf("elf: clearing BSS for program header %zu at 0x%p\n", i, base);
			ret = memset_user(base, 0, data->phdrs[i].p_memsz - data->phdrs[i].p_filesz);
			if(ret != 0) {
				return ret;
			}

			break;
		}
	}

	/* Copy arguments/environment. */
	elf_binary_copy_data(data);
	return 0;
}

/** Clean up ELF loader data.
 * @param binary	Binary loader data structure. */
static void elf_binary_cleanup(loader_binary_t *binary) {
	elf_binary_t *data = binary->data;

	kfree(data->phdrs);
	kfree(data);
}

/** ELF binary type structure. */
static loader_type_t elf_binary_type = {
	.name = "ELF",
	.check = elf_binary_check,
	.load = elf_binary_load,
	.finish = elf_binary_finish,
	.cleanup = elf_binary_cleanup,
};

/** Initialization function to register the ELF binary type. */
static void __init_text elf_init(void) {
	if(loader_type_register(&elf_binary_type) != 0) {
		fatal("Could not register ELF binary type");
	}
}
INITCALL(elf_init);

#if 0
# pragma mark ELF module loader.
#endif

#undef dprintf
#if CONFIG_LOADER_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern bool elf_module_check(vfs_node_t *node);
extern int elf_module_get_sym(module_t *module, size_t num, bool external, elf_addr_t *valp);
extern int elf_module_load(module_t *module);
extern int elf_module_relocate(module_t *module, bool external);
extern void *module_mem_alloc(size_t size, int mmflag);

/** Check whether a file is an ELF module.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the file is an ELF module. */
bool elf_module_check(vfs_node_t *node) {
	return elf_check(node, ELF_ET_REL);
}

/** Find a section in an ELF module.
 * @param module	Module to find in.
 * @param name		Name of section to find.
 * @return		Pointer to section or NULL if not found. */
static elf_shdr_t *elf_module_find_section(module_t *module, const char *name) {
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

/** Get value of a symbol from a module.
 * @param module	Module to get value from.
 * @param num		Number of the symbol.
 * @param external	Whether to handle external or internal symbols.
 * @param valp		Where to store symbol value.
 * @return		1 on success, 0 if lookup not done, negative error
 *			code on failure. */
int elf_module_get_sym(module_t *module, size_t num, bool external, elf_addr_t *valp) {
	const char *strtab;
	elf_shdr_t *symtab;
	elf_sym_t *sym;
	symbol_t *ksym;

	if(!(symtab = elf_module_find_section(module, ".symtab"))) {
		return -ERR_FORMAT_INVAL;
	} else if(num >= (symtab->sh_size / symtab->sh_entsize)) {
		return -ERR_FORMAT_INVAL;
	}

	strtab = (const char *)MODULE_ELF_SECT(module, symtab->sh_link)->sh_addr;
	sym = (elf_sym_t *)(symtab->sh_addr + (symtab->sh_entsize * num));
	if(sym->st_shndx == ELF_SHN_UNDEF) {
		if(!external) {
			return 0;
		}

		/* External symbol, look up in the kernel and other modules. */
		if(!(ksym = symbol_lookup_name(strtab + sym->st_name, true, true))) {
			kprintf(LOG_DEBUG, "elf: module references undefined symbol: %s\n", strtab + sym->st_name);
			return -ERR_FORMAT_INVAL;
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

/** Allocate memory for all loadable sections and load them.
 * @param module	Module structure.
 * @return		0 on success, negative error code on failure. */
static int elf_module_load_sections(module_t *module) {
	elf_shdr_t *sect;
	size_t i, bytes;
	void *dest;
	int ret;

	/* Calculate the total size. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);

		switch(sect->sh_type) {
		case ELF_SHT_PROGBITS:
		case ELF_SHT_NOBITS:
		case ELF_SHT_STRTAB:
		case ELF_SHT_SYMTAB:
			if(sect->sh_addralign) {
				module->load_size = ROUND_UP(module->load_size, sect->sh_addralign);
			}
			module->load_size += sect->sh_size;
			break;
		}
	}

	if(module->load_size == 0) {
		dprintf("elf: no loadable sections in module %p\n", module);
		return -ERR_FORMAT_INVAL;
	}

	/* Allocate space to load the module into. */
	module->load_base = dest = module_mem_alloc(module->load_size, MM_SLEEP);

	/* For each section, read its data into the allocated area. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);

		switch(sect->sh_type) {
		case ELF_SHT_NOBITS:
			if(sect->sh_addralign) {
				dest = (void *)ROUND_UP((ptr_t)dest, sect->sh_addralign);
			}
			sect->sh_addr = (elf_addr_t)dest;

			memset(dest, 0, sect->sh_size);
			dest += sect->sh_size;
			break;
		case ELF_SHT_PROGBITS:
		case ELF_SHT_STRTAB:
		case ELF_SHT_SYMTAB:
			if(sect->sh_addralign) {
				dest = (void *)ROUND_UP((ptr_t)dest, sect->sh_addralign);
			}
			sect->sh_addr = (elf_addr_t)dest;

			dprintf("elf: loading data for section %u to %p (size: %u, type: %u)\n",
			        i, dest, sect->sh_size, sect->sh_type);

			/* Read the section data in. */
			if((ret = vfs_file_read(module->node, dest, sect->sh_size, sect->sh_offset, &bytes)) != 0) {
				return ret;
			} else if(bytes != sect->sh_size) {
				return -ERR_FORMAT_INVAL;
			}

			dest += sect->sh_size;
			break;
		}
	}

	return 0;
}

/** Fix and load symbols in an ELF module.
 * @param module	Module to add symbols for.
 * @return		0 on success, negative error code on failure. */
static int elf_module_load_symbols(module_t *module) {
	elf_shdr_t *symtab, *sect;
	const char *strtab;
	elf_sym_t *sym;
	size_t i;

	/* Try to find the symbol table section. */
	if(!(symtab = elf_module_find_section(module, ".symtab"))) {
		dprintf("elf: module does not contain a symbol table\n");
		return -ERR_FORMAT_INVAL;
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

		dprintf("elf: added symbol %s to module %p (addr: %p, size: %p)\n",
			strtab + sym->st_name, module, sym->st_value, sym->st_size);
	}

	return 0;
}

/** Load an ELF kernel module.
 * @param module	Structure describing the module to load.
 * @return		0 on success, negative error code on failure. */
int elf_module_load(module_t *module) {
	size_t size, i, bytes;
	elf_shdr_t *exports;
	const char *export;
	symbol_t *sym;
	int ret;

	/* Read the ELF header in from the file. */
	if((ret = vfs_file_read(module->node, &module->ehdr, sizeof(elf_ehdr_t), 0, &bytes)) != 0) {
		return ret;
	} else if(bytes != sizeof(elf_ehdr_t)) {
		return -ERR_FORMAT_INVAL;
	}

	/* Calculate the size of the section headers and allocate space. */
	size = module->ehdr.e_shnum * module->ehdr.e_shentsize;
	module->shdrs = kmalloc(size, MM_SLEEP);

	/* Read the headers in. */
	if((ret = vfs_file_read(module->node, module->shdrs, size, module->ehdr.e_shoff, &bytes)) != 0) {
		return ret;
	} else if(bytes != size) {
		return -ERR_FORMAT_INVAL;
	}

	/* Load all loadable sections into memory, populate the symbol table
	 * and perform internal relocations. */
	if((ret = elf_module_load_sections(module)) != 0) {
		return ret;
	} else if((ret = elf_module_load_symbols(module)) != 0) {
		return ret;
	} else if((ret = elf_module_relocate(module, false)) != 0) {
		return ret;
	}

	/* If there is an exports section, export all symbols defined in it. */
	if((exports = elf_module_find_section(module, ".modexports"))) {
		for(i = 0; i < exports->sh_size; i += sizeof(const char *)) {
			export = (const char *)(*(ptr_t *)(exports->sh_addr + i));

			/* Find the symbol and mark it as exported. */
			sym = symbol_table_lookup_name(&module->symtab, export, true, false);
			if(sym == NULL) {
				dprintf("module: exported symbol %p in module %p cannot be found\n",
				        export, module);
				return -ERR_FORMAT_INVAL;
			}

			sym->exported = true;

			dprintf("module: exported symbol %s in module %p\n", export, module);
		}
	}

	return 0;
}
