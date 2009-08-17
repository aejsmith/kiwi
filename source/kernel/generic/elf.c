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

#include <proc/process.h>

#include <elf.h>
#include <errors.h>
#include <fatal.h>
#include <module.h>

/** Check whether an ELF header is valid.
 * @param ehdr		Executable header.
 * @param type		Required ELF type.
 * @return		True if valid, false if not. */
static bool elf_check_ehdr(elf_ehdr_t *ehdr, int type) {
	/* Check the magic number and version. */
	if(strncmp((const char *)ehdr->e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
		return false;
	} else if(ehdr->e_ident[ELF_EI_VERSION] != 1 || ehdr->e_version != 1) {
		return false;
	}

	/* Check whether it matches the architecture we're running on. */
	if(ehdr->e_ident[ELF_EI_CLASS] != ELF_CLASS ||
           ehdr->e_ident[ELF_EI_DATA] != ELF_ENDIAN ||
	   ehdr->e_machine != ELF_MACHINE) {
		return false;
	}

	/* Finally check type of binary. */
	return (ehdr->e_type == type);
}

/** Check whether an FS node contains a valid ELF header.
 * @param node		Filesystem node.
 * @param type		Required ELF type.
 * @return		True if valid, false if not. */
static bool elf_check_node(vfs_node_t *node, int type) {
	elf_ehdr_t ehdr;
	size_t bytes;
	int ret;

	/* Read the ELF header in from the file. */
	if((ret = vfs_file_read(node, &ehdr, sizeof(elf_ehdr_t), 0, &bytes)) != 0) {
		return false;
	} else if(bytes != sizeof(elf_ehdr_t)) {
		return false;
	}

	return elf_check_ehdr(&ehdr, type);
}

#if 0
# pragma mark ELF executable loader.
#endif

#if CONFIG_PROC_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** ELF loader binary data structure. */
typedef struct elf_binary {
	elf_ehdr_t ehdr;		/**< Executable header. */
	elf_phdr_t *phdrs;		/**< Program headers. */
	vfs_node_t *node;		/**< Node being loaded. */
	vm_aspace_t *as;		/**< Address space to map in to. */
} elf_binary_t;

/** Reserve space for a binary in an address space.
 * @param binary	Binary to reserve space for.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_reserve_space(elf_binary_t *binary) {
	ptr_t start, end;
	size_t i, size;
	int ret;

	for(i = 0; i < binary->ehdr.e_phnum; i++) {
		if(binary->phdrs[i].p_type != ELF_PT_LOAD) {
			continue;
		}

		start = ROUND_DOWN(binary->phdrs[i].p_vaddr, PAGE_SIZE);
		end = ROUND_UP(binary->phdrs[i].p_vaddr + binary->phdrs[i].p_memsz, PAGE_SIZE);
		size = end - start;

		if((ret = vm_reserve(binary->as, start, size)) != 0) {
			return ret;
		}
	}

	return 0;
}

/** Handle an ELF_PT_LOAD program header.
 * @param binary	ELF binary data structure.
 * @param phdr		Program header to load.
 * @param i		Index of program header.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_phdr_load(elf_binary_t *binary, elf_phdr_t *phdr, size_t i) {
	int ret, flags = 0;
	ptr_t start, end;
	offset_t offset;
	size_t size;

	/* Work out the protection flags to use. */
	flags |= ((phdr->p_flags & ELF_PF_R) ? VM_MAP_READ  : 0);
	flags |= ((phdr->p_flags & ELF_PF_W) ? VM_MAP_WRITE : 0);
	flags |= ((phdr->p_flags & ELF_PF_X) ? VM_MAP_EXEC  : 0);
	if(flags == 0) {
		dprintf("elf: program header %zu has no protection flags set\n", i);
		return -ERR_FORMAT_INVAL;
	}

	/* Set the private and fixed flags - we always want to insert at the
	 * position we say, and not share stuff. */
	flags |= (VM_MAP_FIXED | VM_MAP_PRIVATE);

	/* Map an anonymous region if memory size is greater than file size. */
	if(phdr->p_memsz > phdr->p_filesz) {
		start = ROUND_DOWN(phdr->p_vaddr + phdr->p_filesz, PAGE_SIZE);
		end = ROUND_UP(phdr->p_vaddr + phdr->p_memsz, PAGE_SIZE);
		size = end - start;

		dprintf("elf: loading BSS for %zu to %p (size: %zu)\n", i, start, size);

		/* We have to have it writeable for us to be able to clear it
		 * later on. */
		if((flags & VM_MAP_WRITE) == 0) {
			dprintf("elf: program header %zu should be writeable\n", i);
			return -ERR_FORMAT_INVAL;
		}

		/* Create an anonymous memory region for it. */
		if((ret = vm_map_anon(binary->as, start, size, flags, NULL)) != 0) {
			return ret;
		}
	}

	/* If file size is zero then this header is just uninitialized data. */
	if(phdr->p_filesz == 0) {
		return 0;
	}

	/* Work out the address to map to and the offset in the file. */
	start = ROUND_DOWN(phdr->p_vaddr, PAGE_SIZE);
	end = ROUND_UP(phdr->p_vaddr + phdr->p_filesz, PAGE_SIZE);
	size = end - start;
	offset = ROUND_DOWN(phdr->p_offset, PAGE_SIZE);

	dprintf("elf: loading program header %zu to %p (size: %zu)\n", i, start, size);

	/* Map the data in. We do not need to check whether the supplied
	 * addresses are valid - vm_map_file() will reject the call if they
	 * are. */
	return vm_map_file(binary->as, start, size, flags, binary->node, offset, NULL);
}

/** Check whether a binary is an ELF binary.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the binary is an ELF binary. */
bool elf_binary_check(vfs_node_t *node) {
	return elf_check_node(node, ELF_ET_EXEC);
}

/** Load an ELF binary into an address space.
 * @param node		Filesystem node being loaded.
 * @param as		Address space to load into.
 * @param interp	Whether this binary is an interpreter (to prevent an
 *			interpreter requiring an interpreter).
 * @param datap		Where to store data pointer to pass to other functions.
 * @return		0 on success, negative error code on failure. */
static int elf_binary_load_internal(vfs_node_t *node, vm_aspace_t *as, bool interp, void **datap) {
	size_t bytes, i, size, load_count = 0;
	elf_binary_t *binary;
	char *path;
	int ret;

	/* Allocate a structure to store data about the binary. */
	binary = kmalloc(sizeof(elf_binary_t), MM_SLEEP);
	binary->phdrs = NULL;
	binary->node = node;
	binary->as = as;

	/* Read in the ELF header and check it. */
	if((ret = vfs_file_read(node, &binary->ehdr, sizeof(elf_ehdr_t), 0, &bytes)) != 0) {
		goto fail;
	} else if(bytes != sizeof(elf_ehdr_t)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	} else if(!elf_check_ehdr(&binary->ehdr, ELF_ET_EXEC)) {
		if(interp) {
			dprintf("elf: interpreter %p is not valid ELF file\n", node);
		}
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Check that program headers are the right size... */
	if(binary->ehdr.e_phentsize != sizeof(elf_phdr_t)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Allocate some memory for the program headers and load them too. */
	size = binary->ehdr.e_phnum * binary->ehdr.e_phentsize;
	binary->phdrs = kmalloc(size, MM_SLEEP);
	if((ret = vfs_file_read(node, binary->phdrs, size, binary->ehdr.e_phoff, &bytes)) != 0) {
		goto fail;
	} else if(bytes != size) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Look for an interpreter header, and load the interpreter instead if
	 * there is one. */
	for(i = 0; i < binary->ehdr.e_phnum; i++) {
		if(binary->phdrs[i].p_type != ELF_PT_INTERP) {
			continue;
		} else if(interp) {
			dprintf("elf: interpreter %p requires an interpreter\n", node);
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}

		/* Read in the interpreter path. */
		path = kmalloc(binary->phdrs[i].p_filesz, MM_SLEEP);
		if((ret = vfs_file_read(node, path, binary->phdrs[i].p_filesz, binary->phdrs[i].p_offset, &bytes)) != 0) {
			kfree(path);
			goto fail;
		} else if(bytes != binary->phdrs[i].p_filesz) {
			kfree(path);
			ret = -ERR_FORMAT_INVAL;
			goto fail;
		}
		dprintf("elf: %p has interpreter %s\n", node, path);

		/* Reserve space for the real binary to be loaded into, so that
		 * the VM system doesn't put the stack or argument block where
		 * the binary needs to go. */
		if((ret = elf_binary_reserve_space(binary)) != 0) {
			kfree(path);
			goto fail;
		}

		/* Clean up old state data. */
		kfree(binary->phdrs);
		kfree(binary);

		/* Look up the interpreter on the FS. */
		ret = vfs_node_lookup(path, true, VFS_NODE_FILE, &node);
		kfree(path);
		if(ret != 0) {
			return ret;
		}

		ret = elf_binary_load_internal(node, as, true, datap);
		vfs_node_release(node);
		return 0;
	}

	/* Handle all the program headers. */
	for(i = 0; i < binary->ehdr.e_phnum; i++) {
		switch(binary->phdrs[i].p_type) {
		case ELF_PT_LOAD:
			if((ret = elf_binary_phdr_load(binary, &binary->phdrs[i], i)) != 0) {
				goto fail;
			}
			load_count++;
			break;
		case ELF_PT_DYNAMIC:
		case ELF_PT_PHDR:
		case ELF_PT_NOTE:
			/* These can be ignored without warning. */
			break;
		default:
			dprintf("elf: unknown program header type %u, ignoring\n", binary->phdrs[i].p_type);
			break;
		}
	}

	/* Check if we actually loaded anything. */
	if(!load_count) {
		dprintf("elf: binary %p did not have any loadable program headers\n", node);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	*datap = binary;
	return 0;
fail:
	if(binary->phdrs) {
		kfree(binary->phdrs);
	}
	kfree(binary);
	return ret;
}

/** Load an ELF binary into an address space.
 * @param node		Filesystem node being loaded.
 * @param as		Address space to load into.
 * @param datap		Where to store data pointer to pass to other functions.
 * @return		0 on success, negative error code on failure. */
int elf_binary_load(vfs_node_t *node, vm_aspace_t *as, void **datap) {
	return elf_binary_load_internal(node, as, false, datap);
}

/** Finish binary loading, after address space is switched.
 * @param data		Data pointer returned from elf_binary_load().
 * @return		Address of program entry point. */
ptr_t elf_binary_finish(void *data) {
	elf_binary_t *binary = (elf_binary_t *)data;
	void *base;
	size_t i;

	/* Clear the BSS sections. */
	for(i = 0; i < binary->ehdr.e_phnum; i++) {
		switch(binary->phdrs[i].p_type) {
		case ELF_PT_LOAD:
			if(binary->phdrs[i].p_filesz >= binary->phdrs[i].p_memsz) {
				break;
			}

			base = (void *)(binary->phdrs[i].p_vaddr + binary->phdrs[i].p_filesz);
			dprintf("elf: clearing BSS for program header %zu at %p\n", i, base);
			memset(base, 0, binary->phdrs[i].p_memsz - binary->phdrs[i].p_filesz);
			break;
		}
	}

	return (ptr_t)binary->ehdr.e_entry;
}

/** Clean up ELF loader data.
 * @param data		Data pointer returned from elf_binary_load(). */
void elf_binary_cleanup(void *data) {
	elf_binary_t *binary = data;

	kfree(binary->phdrs);
	kfree(binary);
}

#if 0
# pragma mark ELF module loader.
#endif

#undef dprintf
#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern int elf_module_get_sym(module_t *module, size_t num, bool external, elf_addr_t *valp);

/** Check whether a file is an ELF module.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the file is an ELF module. */
bool elf_module_check(vfs_node_t *node) {
	return elf_check_node(node, ELF_ET_REL);
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
	} else if(!elf_check_ehdr(&module->ehdr, ELF_ET_EXEC)) {
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
