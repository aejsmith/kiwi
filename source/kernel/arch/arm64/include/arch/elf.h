/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               ARM64 ELF definitions.
 */

#pragma once

/** Definitions of ELF machine type, endianness, etc. */
#define ELF_MACHINE ELF_EM_AARCH64
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
