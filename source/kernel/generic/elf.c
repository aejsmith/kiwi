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

#if 0
# pragma mark Executable loader type.
#endif

#if CONFIG_LOADER_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** ELF loader binary data structure. */
typedef struct elf_binary {
	elf_ehdr_t ehdr;		/**< ELF executable header. */
	elf_phdr_t *phdrs;		/**< Program headers. */

	loader_binary_t *binary;	/**< Pointer back to the loader's binary structure. */
} elf_binary_t;

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
 * @return		Whether the binary is an ELF file. */
static bool elf_binary_check(vfs_node_t *node) {
	elf_ehdr_t ehdr;
	size_t bytes;
	int ret;

	/* Read the ELF header in from the file. */
	ret = vfs_file_read(node, &ehdr, sizeof(elf_ehdr_t), 0, &bytes);
	if(ret != 0) {
		return false;
	}

	/* Check if this is a valid ELF image. Pass the bytes count into the
	 * ELF check function, which will check for us if it is large enough. */
	return elf_check(&ehdr, bytes, ELF_ET_EXEC);
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
	ret = vfs_file_read(binary->node, &data->ehdr, sizeof(elf_ehdr_t), 0, &bytes);
	if(ret != 0) {
		goto fail;
	} else if(!elf_check(&data->ehdr, bytes, ELF_ET_EXEC)) {
		/* This can happen if some sneaky bugger changes the file
		 * between checking it and getting here. */
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
# pragma mark Utility functions.
#endif

/** Check whether a memory buffer contains a valid ELF header.
 * @param image		Pointer to image.
 * @param size		Size of memory image.
 * @param type		Required ELF binary type.
 * @return		True if valid, false if not. */
bool elf_check(void *image, size_t size, int type) {
	elf_ehdr_t *ehdr = image;

	/* Reject images that are too small. */
	if(size < sizeof(elf_ehdr_t)) {
		return false;
	}

	/* Check the magic number and version. */
	if(strncmp((const char *)ehdr->e_ident, ELF_MAGIC, strlen(ELF_MAGIC)) != 0) {
		return false;
	} else if(ehdr->e_ident[ELF_EI_VERSION] != 1 || ehdr->e_version != 1) {
		return false;
	}

	/* Check whether it matches the architecture we're running on. */
	if(ehdr->e_ident[ELF_EI_CLASS] != ELF_CLASS || ehdr->e_ident[ELF_EI_DATA] != ELF_ENDIAN ||
	   ehdr->e_machine != ELF_MACHINE) {
		return false;
	}

	/* Finally check type of binary. */
	return (ehdr->e_type == type);
}
