/* Kiwi ELF executable loader
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
 * @brief		ELF executable loader.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include "loader_priv.h"

/** ELF loader binary data structure. */
typedef struct elf_binary {
	elf_ehdr_t ehdr;		/**< ELF executable header. */
	elf_phdr_t *phdrs;		/**< Program headers. */
	loader_elf_abi_t *abi;		/**< ABI of the binary. */

	loader_binary_t *binary;	/**< Pointer back to the loader's binary structure. */
} elf_binary_t;

/** List of known ELF ABI types. */
static LIST_DECLARE(elf_abi_list);
static MUTEX_DECLARE(elf_abi_list_lock, 0);

/** Work out the ABI type of an ELF binary.
 * @param data		ELF binary structure.
 * @return		Pointer to ABI type structure (NULL if ABI unknown). */
static loader_elf_abi_t *loader_elf_abi_match(elf_binary_t *data) {
	loader_elf_abi_t *abi;
	size_t i;

	mutex_lock(&elf_abi_list_lock, 0);

	/* First see if we have an ABI note. */
	for(i = 0; i < data->ehdr.e_phnum; i++) {
		if(data->phdrs[i].p_type != ELF_PT_NOTE) {
			continue;
		}

		fatal("TODO: Note ABI identification");
		/* Read in the note data. */
		
		/* Check if this note is labelled Kiwi, type 1. */
	}

	/* No note was found, fallback on the OSABI field. */
	LIST_FOREACH(&elf_abi_list, iter) {
		abi = list_entry(iter, loader_elf_abi_t, header);

		if(abi->num >= 0 && data->ehdr.e_ident[ELF_EI_OSABI] == (unsigned char)abi->num) {
			dprintf("loader: matched binary '%s' to ABI type %s (%d)\n",
			        data->binary->node->name, abi->string, abi->num);
			mutex_unlock(&elf_abi_list_lock);
			return abi;
		}
	}

	mutex_unlock(&elf_abi_list_lock);
	return NULL;
}

/** Handle an ELF_PT_LOAD program header.
 * @param data		ELF binary data structure.
 * @param i		Index of program header.
 * @return		0 on success, negative error code on failure. */
static int loader_elf_phdr_load(elf_binary_t *data, size_t i) {
	aspace_source_t *source;
	int ret, flags = 0;
	ptr_t start, end;
	offset_t offset;
	size_t size;

	/* Work out the protection flags to use. */
	flags |= ((data->phdrs[i].p_flags & ELF_PF_X) ? AS_REGION_EXEC : 0);
	flags |= ((data->phdrs[i].p_flags & ELF_PF_W) ? AS_REGION_WRITE : 0);
	flags |= ((data->phdrs[i].p_flags & ELF_PF_R) ? AS_REGION_READ  : 0);
	if(flags == 0) {
		dprintf("loader: PHDR %" PRIs " has no protection flags set\n", i);
		return -ERR_OBJ_FORMAT_BAD;
	}

	/* Map the BSS if required. */
	if(data->phdrs[i].p_filesz != data->phdrs[i].p_memsz) {
		start = ROUND_DOWN(data->phdrs[i].p_vaddr + data->phdrs[i].p_filesz, PAGE_SIZE);
		end = ROUND_UP(data->phdrs[i].p_vaddr + data->phdrs[i].p_memsz, PAGE_SIZE);
		size = end - start;

		dprintf("loader: loading BSS for %" PRIs " to %p (size: %" PRIs ")\n", i, start, size);

		/* We have to have it writeable for us to be able to clear it
		 * later on. */
		if((flags & AS_REGION_WRITE) == 0) {
			dprintf("loader: PHDR %" PRIs " should be writeable\n", i);
			return -ERR_OBJ_FORMAT_BAD;
		}

		/* Create an anonymous memory region for it. */
		ret = aspace_anon_create(AS_SOURCE_PRIVATE, &source);
		if(ret != 0) {
			return ret;
		}

		ret = aspace_insert(data->binary->aspace, start, size, flags, source, 0);
		if(ret != 0) {
			aspace_source_destroy(source);
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

	dprintf("elf: loading PHDR %" PRIs " to %p (size: %" PRIs ")\n", i, start, size);

	/* Map the data in. We do not need to check whether the supplied
	 * addresses are valid - aspace_insert() will reject the call if they
	 * are. */
	ret = vfs_node_aspace_create(data->binary->node, AS_SOURCE_PRIVATE, &source);
	if(ret != 0) {
		return ret;
	}

	ret = aspace_insert(data->binary->aspace, start, size, flags, source, offset);
	if(ret != 0) {
		aspace_source_destroy(source);
		return ret;
	}

	return 0;
}

/** Check whether a binary is an ELF binary.
 * @param node		Filesystem node referring to the binary.
 * @return		Whether the binary is an ELF file. */
static bool loader_elf_check(vfs_node_t *node) {
	elf_ehdr_t ehdr;
	size_t bytes;
	int ret;

	/* Read the ELF header in from the file. */
	ret = vfs_node_read(node, &ehdr, sizeof(elf_ehdr_t), 0, &bytes);
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
static int loader_elf_load(loader_binary_t *binary) {
	size_t bytes, size, i, load_count = 0;
	elf_binary_t *data;
	int ret;

	/* Allocate a structure to store data about the binary. */
	data = kmalloc(sizeof(elf_binary_t), MM_SLEEP);
	data->phdrs = NULL;
	data->binary = binary;

	/* Read in the ELF header. */
	ret = vfs_node_read(binary->node, &data->ehdr, sizeof(elf_ehdr_t), 0, &bytes);
	if(ret != 0) {
		goto fail;
	} else if(!elf_check(&data->ehdr, bytes, ELF_ET_EXEC)) {
		/* This can happen if some sneaky bugger changes the file
		 * between checking it and getting here. */
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	/* Check that program headers are the right size... */
	if(data->ehdr.e_phentsize != sizeof(elf_phdr_t)) {
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	/* Allocate some memory for the program headers and load them too. */
	size = data->ehdr.e_phnum * data->ehdr.e_phentsize;
	data->phdrs = kmalloc(size, MM_SLEEP);
	ret = vfs_node_read(binary->node, data->phdrs, size, data->ehdr.e_phoff, &bytes);
	if(ret != 0) {
		goto fail;
	} else if(bytes != size) {
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	/* We now have enough information to work out the binary's ABI. */
	data->abi = loader_elf_abi_match(data);
	if(!data->abi) {
		kprintf(LOG_DEBUG, "loader: unknown ELF ABI type for binary '%s'\n", binary->node->name);
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	/* Handle all the program headers. */
	for(i = 0; i < data->ehdr.e_phnum; i++) {
		switch(data->phdrs[i].p_type) {
		case ELF_PT_LOAD:
			ret = loader_elf_phdr_load(data, i);
			if(ret != 0) {
				goto fail;
			}
			load_count++;
			break;
		case ELF_PT_NOTE:
			/* These can be ignored without warning. */
			break;
		default:
			dprintf("loader: unknown ELF PHDR type %u, ignoring\n", data->phdrs[i].p_type);
			break;
		}
	}

	/* Check if we actually loaded anything. */
	if(!load_count) {
		dprintf("loader: ELF binary '%s' did not have any loadable program headers\n",
		        binary->node->name);
		ret = -ERR_OBJ_FORMAT_BAD;
		goto fail;
	}

	binary->data = data;
	binary->subsystem = &data->abi->subsystem;
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
static int loader_elf_finish(loader_binary_t *binary) {
	return 0;
}

/** Clean up ELF loader data.
 * @param binary	Binary loader data structure. */
static void loader_elf_cleanup(loader_binary_t *binary) {
	elf_binary_t *data = binary->data;

	kfree(data->phdrs);
	kfree(data);
}

/** ELF executable loader type. */
loader_type_t loader_elf_type = {
	.name = "ELF",
	.check = loader_elf_check,
	.load = loader_elf_load,
	.finish = loader_elf_finish,
	.cleanup = loader_elf_cleanup,
};

/** Register an ELF ABI type.
 *
 * Registers an ELF ABI type with the loader. This system allows for multiple
 * subsystems based on ELF, and makes it easy to choose which subsystem to
 * run a binary on. There are two methods for matching a binary to an ABI. If
 * a binary provides a note (name Kiwi, type 1), then the note specifies the
 * name of an ABI to use. If a note is not specified, then the loader will
 * attempt to match the binary's OS/ABI field in the ELF header to an ABI.
 *
 * @param abi		ABI to register.
 *
 * @return		0 on success, negative error code on failure.
 */
int loader_elf_abi_register(loader_elf_abi_t *abi) {
	loader_elf_abi_t *exist;

	if(!abi->string) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&elf_abi_list_lock, 0);

	/* Check if the ABI already exists. */
	LIST_FOREACH(&elf_abi_list, iter) {
		exist = list_entry(iter, loader_elf_abi_t, header);

		if(exist->string && abi->string && (strcmp(exist->string, abi->string) == 0)) {
			mutex_unlock(&elf_abi_list_lock);
			return -ERR_OBJ_EXISTS;
		} else if(exist->num != -1 && exist->num == abi->num) {
			mutex_unlock(&elf_abi_list_lock);
			return -ERR_OBJ_EXISTS;
		}
	}

	list_init(&abi->header);
	list_append(&elf_abi_list, &abi->header);

	dprintf("loader: registered ELF ABI type %p(%s:%d)\n", abi, abi->string, abi->num);
	mutex_unlock(&elf_abi_list_lock);
	return 0;
}
MODULE_EXPORT(loader_elf_abi_register);

/** Remove an ELF ABI type.
 *
 * Removes an ELF ABI type from the ABI type list.
 *
 * @param abi		ABI to remove.
 */
void loader_elf_abi_unregister(loader_elf_abi_t *abi) {
	mutex_lock(&elf_abi_list_lock, 0);
	list_remove(&abi->header);
	mutex_unlock(&elf_abi_list_lock);
}
MODULE_EXPORT(loader_elf_abi_unregister);
