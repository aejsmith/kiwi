/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 kernel library relocation function.
 */

#include "libkernel.h"

/** Relocate the library.
 * @param args          Process argument block.
 * @param dyn           Pointer to dynamic section. */
void libkernel_relocate(process_args_t *args, elf_dyn_t *dyn) {
    elf_rela_t *reloc = NULL;
    size_t size = 0;
    size_t ent = 0;

    elf_addr_t load_base = (elf_addr_t)args->load_base;

    for (size_t i = 0; dyn[i].d_tag != ELF_DT_NULL; ++i) {
        switch (dyn[i].d_tag) {
            case ELF_DT_RELA:
                reloc = (elf_rela_t *)(dyn[i].d_un.d_ptr + load_base);
                break;
            case ELF_DT_RELASZ:
                size = dyn[i].d_un.d_val;
                break;
            case ELF_DT_RELAENT:
                ent = dyn[i].d_un.d_val;
                break;
        }
    }

    for (size_t i = 0; i < size / ent; i++, reloc = (elf_rela_t *)((ptr_t)reloc + ent)) {
        elf_addr_t *addr = (elf_addr_t *)(load_base + reloc->r_offset);

        switch (ELF64_R_TYPE(reloc->r_info)) {
            case ELF_R_X86_64_RELATIVE:
                *addr = (elf_addr_t)load_base + reloc->r_addend;
                break;
            case ELF_R_X86_64_DTPMOD64:
                *addr = LIBKERNEL_IMAGE_ID;
                break;
            default:
                kern_process_exit(STATUS_MALFORMED_IMAGE);
                break;
        }
    }
}
