/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 kernel library relocation function.
 *
 * Reference:
 *  - ELF for the Arm 64-bit Architecture (AArch64)
 *    https://github.com/ARM-software/abi-aa/blob/main/aaelf64/aaelf64.rst
 */

#include "libkernel.h"

static void do_relocations(elf_rela_t *reloc, size_t size, size_t ent, elf_addr_t load_base) {
    for (size_t i = 0; i < size / ent; i++, reloc = (elf_rela_t *)((ptr_t)reloc + ent)) {
        elf_addr_t *addr = (elf_addr_t *)(load_base + reloc->r_offset);

        switch (ELF64_R_TYPE(reloc->r_info)) {
            case ELF_R_AARCH64_RELATIVE:
                *addr = (elf_addr_t)load_base + reloc->r_addend;
                break;
            case ELF_R_AARCH64_TLSDESC:
// TODO  
                break;
            default:
                kern_process_exit(STATUS_MALFORMED_IMAGE);
                break;
        }
    }
}

/** Relocate the library.
 * @param args          Process argument block.
 * @param dyn           Pointer to dynamic section. */
void libkernel_relocate(process_args_t *args, elf_dyn_t *dyn) {
    elf_rela_t *rela = NULL;
    size_t rela_size = 0;
    size_t rela_ent = 0;

    elf_rela_t *plt = NULL;
    size_t plt_size = 0;

    elf_addr_t load_base = (elf_addr_t)args->load_base;

    for (size_t i = 0; dyn[i].d_tag != ELF_DT_NULL; ++i) {
        switch (dyn[i].d_tag) {
            case ELF_DT_RELA:
                rela = (elf_rela_t *)(dyn[i].d_un.d_ptr + load_base);
                break;
            case ELF_DT_RELASZ:
                rela_size = dyn[i].d_un.d_val;
                break;
            case ELF_DT_RELAENT:
                rela_ent = dyn[i].d_un.d_val;
                break;
            case ELF_DT_JMPREL:
                plt = (elf_rela_t *)(dyn[i].d_un.d_ptr + load_base);
                break;
            case ELF_DT_PLTRELSZ:
                plt_size = dyn[i].d_un.d_val;
                break;
        }
    }

    if (rela)
        do_relocations(rela, rela_size, rela_ent, load_base);
    if (plt)
        do_relocations(plt, plt_size, rela_ent, load_base);
}
