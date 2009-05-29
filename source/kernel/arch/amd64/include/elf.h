/* Kiwi AMD64 ELF definitions
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
 * @brief		AMD64 ELF definitions.
 */

#ifndef __ARCH_ELF_H
#define __ARCH_ELF_H

#ifndef __ELF_H
# error "Do not include this file directly; use elf.h instead"
#endif

/** Definitions of ELF machine type, endianness, etc. */
#define ELF_MACHINE	ELF_EM_X86_64	/**< ELF machine (x86_64). */
#define ELF_CLASS	ELFCLASS64	/**< ELF class (64-bit). */
#define ELF_ENDIAN	ELFDATA2LSB	/**< ELF endianness (little-endian). */

/** Type definitions to select the right structures/types to use. */
typedef Elf64_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf64_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf64_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf64_Sym  elf_sym_t;		/**< ELF symbol structure. */

#endif /* __ARCH_ELF_H */
