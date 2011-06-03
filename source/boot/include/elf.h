/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		ELF loading functions.
 */

#ifndef __BOOT_ELF_H
#define __BOOT_ELF_H

#include "../../include/elf.h"

#include <arch/mmu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <console.h>
#include <fs.h>
#include <memory.h>

#ifdef CONFIG_ARCH_LITTLE_ENDIAN
# define ELF_ENDIAN	ELFDATA2LSB
#else
# define ELF_ENDIAN	ELFDATA2MSB
#endif

/** Check whether a file is a certain ELF type.
 * @param handle	Handle to file to check.
 * @param bitsize	ELF class definition.
 * @param machine	ELF machine definition.
 * @return		Whether the file is this type. */
static inline bool elf_check(fs_handle_t *handle, uint8_t bitsize, uint8_t machine) {
	Elf32_Ehdr ehdr;

	if(!fs_file_read(handle, &ehdr, sizeof(ehdr), 0)) {
		return false;
	} else if(strncmp((const char *)ehdr.e_ident, ELF_MAGIC, 4) != 0) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_CLASS] != bitsize) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_DATA] != ELF_ENDIAN) {
		return false;
	} else if(machine != ELF_EM_NONE && ehdr.e_machine != machine) {
		return false;
	} else if(ehdr.e_type != ELF_ET_EXEC) {
		return false;
	}
	return true;
}

/** ELF note type. */
typedef Elf32_Note elf_note_t;

/** ELF note iteration function.
 * @param note		Note header.
 * @param name		Note name.
 * @param desc		Note data.
 * @param data		Data pointer passed to elf_note_iterate().
 * @return		Whether to continue iteration. */
typedef bool (*elf_note_iterate_t)(elf_note_t *note, const char *name, void *desc, void *data);

extern bool elf_note_iterate(fs_handle_t *handle, elf_note_iterate_t cb, void *data);

/** Macro expanding to a function to load an ELF kernel.
 * @param _name		Name to give the function.
 * @param _bits		32 or 64.
 * @param _alignment	Alignment for physical memory allocations. */
#define DEFINE_ELF_LOADER(_name, _bits, _alignment)	\
	static inline void _name(fs_handle_t *handle, mmu_context_t *ctx, Elf##_bits##_Addr *entryp, \
			phys_ptr_t *physp) { \
		Elf##_bits##_Addr virt_base = 0, virt_end = 0; \
		Elf##_bits##_Phdr *phdrs; \
		Elf##_bits##_Ehdr ehdr; \
		phys_ptr_t phys; \
		ptr_t dest; \
		size_t i; \
		\
		if(!fs_file_read(handle, &ehdr, sizeof(ehdr), 0)) { \
			boot_error("Could not read kernel image"); \
		} \
		\
		phdrs = kmalloc(sizeof(*phdrs) * ehdr.e_phnum); \
		if(!fs_file_read(handle, phdrs, ehdr.e_phnum * ehdr.e_phentsize, ehdr.e_phoff)) { \
			boot_error("Could not read kernel image"); \
		} \
		\
		for(i = 0; i < ehdr.e_phnum; i++) { \
			if(phdrs[i].p_type != ELF_PT_LOAD) { \
				continue; \
			} \
			if(virt_base == 0 || virt_base > phdrs[i].p_vaddr) { \
				virt_base = phdrs[i].p_vaddr; \
			} \
			if(virt_end < (phdrs[i].p_vaddr + phdrs[i].p_memsz)) { \
				virt_end = phdrs[i].p_vaddr + phdrs[i].p_memsz; \
			} \
		} \
		phys_memory_protect(virt_base, virt_end); \
		\
		phys = phys_memory_alloc(ROUND_UP(virt_end - virt_base, PAGE_SIZE), _alignment, false); \
		dprintf("elf: loading kernel image to 0x%" PRIpp " (size: 0x%zx, align: 0x%zx)\n", \
		        phys, (size_t)(virt_end - virt_base), _alignment); \
		*physp = phys; \
		\
		for(i = 0; i < ehdr.e_phnum; i++) { \
			if(phdrs[i].p_type != ELF_PT_LOAD) { \
				continue; \
			} \
			\
			dest = (ptr_t)(phys + (phdrs[i].p_vaddr - virt_base)); \
			if(phdrs[i].p_filesz) { \
				if(!fs_file_read(handle, (void *)dest, phdrs[i].p_filesz, phdrs[i].p_offset)) { \
					boot_error("Could not read kernel image"); \
				} \
			} \
			\
			memset((void *)(dest + (ptr_t)phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz); \
		} \
		\
		mmu_map(ctx, virt_base, phys, ROUND_UP(virt_end - virt_base, _alignment)); \
		*entryp = ehdr.e_entry; \
	}

#endif /* __BOOT_ELF_H */
