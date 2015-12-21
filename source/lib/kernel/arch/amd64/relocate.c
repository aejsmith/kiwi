/*
 * Copyright (C) 2010-2014 Alex Smith
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
 * @brief               AMD64 kernel library relocation function.
 */

#include "libkernel.h"

/** Relocate the library.
 * @param args          Process argument block.
 * @param dyn           Pointer to dynamic section. */
void libkernel_relocate(process_args_t *args, elf_dyn_t *dyn) {
    elf_rela_t *reloc;
    elf_addr_t *addr, load_base;
    size_t size, ent, i;

    load_base = (elf_addr_t)args->load_base;

    for (i = 0; dyn[i].d_tag != ELF_DT_NULL; ++i) {
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

    for (i = 0; i < size / ent; i++, reloc = (elf_rela_t *)((ptr_t)reloc + ent)) {
        addr = (elf_addr_t *)(load_base + reloc->r_offset);

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
