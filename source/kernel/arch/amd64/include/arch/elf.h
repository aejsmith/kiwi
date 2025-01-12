/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 ELF definitions.
 */

#pragma once

/** Definitions of ELF machine type, endianness, etc. */
#define ELF_MACHINE ELF_EM_X86_64
#define ELF_CLASS   ELFCLASS64
#define ELF_ENDIAN  ELFDATA2LSB

/** Type definitions to select the right structures/types to use. */
typedef Elf64_Ehdr elf_ehdr_t;
typedef Elf64_Phdr elf_phdr_t;
typedef Elf64_Shdr elf_shdr_t;
typedef Elf64_Sym  elf_sym_t;
typedef Elf64_Addr elf_addr_t;
typedef Elf64_Rel  elf_rel_t;
typedef Elf64_Rela elf_rela_t;
