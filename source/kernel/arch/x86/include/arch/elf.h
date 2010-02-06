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
 * @brief		x86 ELF definitions.
 */

#ifndef __ARCH_ELF_H
#define __ARCH_ELF_H

#ifndef __ELF_H
# error "Do not include this file directly; use elf.h instead"
#endif

/** Definitions of ELF machine type, endianness, etc. */
#if __x86_64__
# define ELF_MACHINE	ELF_EM_X86_64	/**< ELF machine (x86_64). */
# define ELF_CLASS	ELFCLASS64	/**< ELF class (64-bit). */
# define ELF_ENDIAN	ELFDATA2LSB	/**< ELF endianness (little-endian). */
#else
# define ELF_MACHINE	ELF_EM_386	/**< ELF machine (i386). */
# define ELF_CLASS	ELFCLASS32	/**< ELF class (32-bit). */
# define ELF_ENDIAN	ELFDATA2LSB	/**< ELF endianness (little-endian). */
#endif

/** Type definitions to select the right structures/types to use. */
#if __x86_64__
typedef Elf64_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf64_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf64_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf64_Sym  elf_sym_t;		/**< ELF symbol structure. */
typedef Elf64_Addr elf_addr_t;		/**< ELF address type. */
#else
typedef Elf32_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf32_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf32_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf32_Sym  elf_sym_t;		/**< ELF symbol structure. */
typedef Elf32_Addr elf_addr_t;		/**< ELF address type. */
#endif

#endif /* __ARCH_ELF_H */
