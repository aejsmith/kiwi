/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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

/** TLS settings. */
#define TLS_VARIANT2		1	/**< Use variant II. */

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
	__asm__ volatile("movl %%gs:0, %0" : "=r"(addr));
	return (tls_tcb_t *)addr;
}

extern void tls_tcb_init(tls_tcb_t *tcb);

#endif /* __LIBKERNEL_ARCH_H */
