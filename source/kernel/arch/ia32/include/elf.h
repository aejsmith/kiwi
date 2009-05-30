/* Kiwi IA32 ELF definitions
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
 * @brief		IA32 ELF definitions.
 */

#ifndef __ARCH_ELF_H
#define __ARCH_ELF_H

#ifndef __ELF_H
# error "Do not include this file directly; use elf.h instead"
#endif

/** Definitions of ELF machine type, endianness, etc. */
#define ELF_MACHINE	ELF_EM_386	/**< ELF machine (i386). */
#define ELF_CLASS	ELFCLASS32	/**< ELF class (32-bit). */
#define ELF_ENDIAN	ELFDATA2LSB	/**< ELF endianness (little-endian). */

/** Type definitions to select the right structures/types to use. */
typedef Elf32_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf32_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf32_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf32_Sym  elf_sym_t;		/**< ELF symbol structure. */

#endif /* __ARCH_ELF_H */
