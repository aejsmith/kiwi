/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IA32 kernel library definitions.
 */

#ifndef __LIBKERNEL_ARCH_H
#define __LIBKERNEL_ARCH_H

#include <elf.h>

/** Relocation types. */
#define ELF_DT_REL_TYPE		ELF_DT_REL
#define ELF_DT_RELSZ_TYPE	ELF_DT_RELSZ

/** Machine type definitions. */
#define ELF_CLASS		ELFCLASS32
#define ELF_ENDIAN		ELFDATA2LSB
#define ELF_MACHINE		ELF_EM_386

/* FIXME: Better place for this. */
#define PAGE_SIZE		0x1000

/** Address type. */
typedef unsigned long ptr_t;

/** ELF type definitions. */
typedef Elf32_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf32_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf32_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf32_Sym  elf_sym_t;		/**< ELF symbol structure. */
typedef Elf32_Addr elf_addr_t;		/**< ELF address type. */
typedef Elf32_Rel  elf_rel_t;		/**< ELF REL type. */
typedef Elf32_Rela elf_rela_t;		/**< ELF RELA type. */
typedef Elf32_Dyn  elf_dyn_t;		/**< ELF dynamic section type. */

#endif /* __LIBKERNEL_ARCH_H */
