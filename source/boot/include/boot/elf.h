/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		ELF loader template class.
 */

#ifndef __BOOT_ELF_H
#define __BOOT_ELF_H

#include <boot/console.h>
#include <boot/memory.h>
#include <boot/vfs.h>

#include <lib/string.h>

#include <elf.h>
#include <fatal.h>
#include <kargs.h>

/** Check whether a file is a certain elf type.
 * @param file		File to check.
 * @param bitsize	ELF class definition.
 * @param endian	ELF endian definition.
 * @param machine	ELF machine definition.
 * @return		Whether the file is this type. */
static inline bool elf_check(vfs_node_t *file, uint8_t bitsize, uint8_t endian, uint8_t machine) {
	Elf32_Ehdr ehdr;

	if(!vfs_file_read(file, &ehdr, sizeof(Elf32_Ehdr), 0)) {
		return false;
	} else if(strncmp((const char *)ehdr.e_ident, ELF_MAGIC, 4) != 0) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_VERSION] != 1 || ehdr.e_version != 1) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_CLASS] != bitsize) {
		return false;
	} else if(ehdr.e_ident[ELF_EI_DATA] != endian) {
		return false;
	} else if(ehdr.e_machine != machine) {
		return false;
	} else if(ehdr.e_type != ELF_ET_EXEC) {
		return false;
	}
	return true;
}

/** Macro expanding to a function to load an ELF kernel.
 * @param _name		Name to give the function.
 * @param _bits		32 or 64.
 * @param _alignment	Alignment for physical memory allocations. */
#define DEFINE_ELF_LOADER(_name, _bits, _alignment)	\
	static inline void _name(vfs_node_t *file, Elf##_bits##_Addr *entryp, \
	                         Elf##_bits##_Addr *virtp, size_t *sizep) { \
		Elf##_bits##_Addr virt_base = 0, virt_end = 0; \
		Elf##_bits##_Phdr *phdrs; \
		Elf##_bits##_Ehdr ehdr; \
		ptr_t dest; \
		size_t i; \
		\
		if(!vfs_file_read(file, &ehdr, sizeof(ehdr), 0)) { \
			fatal("Could not read kernel from boot device"); \
		} \
		\
		phdrs = kmalloc(sizeof(*phdrs) * ehdr.e_phnum); \
		if(!vfs_file_read(file, phdrs, ehdr.e_phnum * ehdr.e_phentsize, ehdr.e_phoff)) { \
			fatal("Could not read kernel from boot device"); \
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
		\
		kernel_args->kernel_phys = phys_memory_alloc(ROUND_UP(virt_end - virt_base, PAGE_SIZE), \
		                                               _alignment, false); \
		dprintf("elf: loading kernel image to 0x%" PRIpp " (size: 0x%zx, align: 0x%zx)\n", \
		        kernel_args->kernel_phys, (size_t)(virt_end - virt_base), _alignment); \
		\
		for(i = 0; i < ehdr.e_phnum; i++) { \
			if(phdrs[i].p_type != ELF_PT_LOAD) { \
				continue; \
			} \
			\
			dest = (ptr_t)(kernel_args->kernel_phys + (phdrs[i].p_vaddr - virt_base)); \
			if(!vfs_file_read(file, (void *)dest, phdrs[i].p_filesz, phdrs[i].p_offset)) { \
				fatal("Could not read kernel from boot device"); \
			} \
			\
			memset((void *)(dest + (ptr_t)phdrs[i].p_filesz), 0, phdrs[i].p_memsz - phdrs[i].p_filesz); \
		} \
		*entryp = ehdr.e_entry; \
		*virtp = virt_base; \
		*sizep = virt_end - virt_base; \
	}

#endif /* __BOOT_ELF_H */
