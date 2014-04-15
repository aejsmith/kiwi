/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		AMD64 kernel library definitions.
 */

#ifndef __LIBKERNEL_ARCH_H
#define __LIBKERNEL_ARCH_H

#include <elf.h>

/** Relocation types. */
#define ELF_DT_REL_TYPE		ELF_DT_RELA
#define ELF_DT_RELSZ_TYPE	ELF_DT_RELASZ

/** Machine type definitions. */
#define ELF_CLASS		ELFCLASS64
#define ELF_ENDIAN		ELFDATA2LSB
#define ELF_MACHINE		ELF_EM_X86_64

/** TLS settings. */
#define TLS_VARIANT2		1	/**< Use variant II. */

/** Address type. */
typedef unsigned long ptr_t;

/** ELF type definitions. */
typedef Elf64_Ehdr elf_ehdr_t;		/**< ELF executable header. */
typedef Elf64_Phdr elf_phdr_t;		/**< ELF program header. */
typedef Elf64_Shdr elf_shdr_t;		/**< ELF section header. */
typedef Elf64_Sym  elf_sym_t;		/**< ELF symbol structure. */
typedef Elf64_Addr elf_addr_t;		/**< ELF address type. */
typedef Elf64_Rel  elf_rel_t;		/**< ELF REL type. */
typedef Elf64_Rela elf_rela_t;		/**< ELF RELA type. */
typedef Elf64_Dyn  elf_dyn_t;		/**< ELF dynamic section type. */

/** TLS thread control block. */
typedef struct tls_tcb {
	void *tpt;			/**< Pointer to this structure. */
	ptr_t *dtv;			/**< Dynamic thread vector. */
	void *base;			/**< Base address of initial TLS allocation. */
} tls_tcb_t;

/** Get a pointer to the current thread's TCB.
 * @return		Pointer to TCB. */
static inline tls_tcb_t *tls_tcb_get(void) {
	unsigned long addr;
	__asm__ volatile("movq %%fs:0, %0" : "=r"(addr));
	return (tls_tcb_t *)addr;
}

extern void tls_tcb_init(tls_tcb_t *tcb);

extern void libkernel_relocate(process_args_t *args, elf_dyn_t *dyn);

#endif /* __LIBKERNEL_ARCH_H */
