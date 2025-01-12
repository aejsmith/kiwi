/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 kernel library definitions.
 */

#pragma once

#include <elf.h>

/** Relocation types. */
#define ELF_DT_REL_TYPE     ELF_DT_RELA
#define ELF_DT_RELSZ_TYPE   ELF_DT_RELASZ

/** Machine type definitions. */
#define ELF_CLASS           ELFCLASS64
#define ELF_ENDIAN          ELFDATA2LSB
#define ELF_MACHINE         ELF_EM_AARCH64

/** TLS settings. */
#define TLS_VARIANT_2       1       /**< Use variant 2. */

/** Address type. */
typedef unsigned long ptr_t;

/** ELF type definitions. */
typedef Elf64_Ehdr elf_ehdr_t;      /**< ELF executable header. */
typedef Elf64_Phdr elf_phdr_t;      /**< ELF program header. */
typedef Elf64_Shdr elf_shdr_t;      /**< ELF section header. */
typedef Elf64_Sym  elf_sym_t;       /**< ELF symbol structure. */
typedef Elf64_Addr elf_addr_t;      /**< ELF address type. */
typedef Elf64_Rel  elf_rel_t;       /**< ELF REL type. */
typedef Elf64_Rela elf_rela_t;      /**< ELF RELA type. */
typedef Elf64_Dyn  elf_dyn_t;       /**< ELF dynamic section type. */

/** TLS thread control block. */
typedef struct tls_tcb {
    void *tpt;                      /**< Pointer to this structure. */
    ptr_t *dtv;                     /**< Dynamic thread vector. */
    void *base;                     /**< Base address of initial TLS allocation. */
} tls_tcb_t;

/** Get a pointer to the current thread's TCB.
 * @return              Pointer to TCB. */
static inline tls_tcb_t *arch_tls_tcb(void) {
    // TODO
    *(int *)0x1234 = 0;
    return NULL;
}

/** Initialise architecture-specific data in the TCB.
 * @param tcb           Thread control block. */
static inline void arch_tls_tcb_init(tls_tcb_t *tcb) {
    // TODO
    *(int *)0x1234 = 0;
}

extern void libkernel_relocate(process_args_t *args, elf_dyn_t *dyn);