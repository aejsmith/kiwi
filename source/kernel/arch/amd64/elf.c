/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               AMD64 ELF helper functions.
 */

#include <lib/utility.h>

#include <elf.h>
#include <kernel.h>
#include <module.h>
#include <status.h>

/** Perform a REL relocation on an ELF module. */
status_t arch_elf_module_relocate_rel(elf_image_t *image, elf_rel_t *rel, elf_shdr_t *target) {
    kprintf(LOG_WARN, "elf: REL relocation section unsupported\n");
    return STATUS_NOT_IMPLEMENTED;
}

/** Perform a RELA relocation on an ELF module. */
status_t arch_elf_module_relocate_rela(elf_image_t *image, elf_rela_t *rel, elf_shdr_t *target) {
    /* Get the location of the relocation. */
    Elf64_Addr *where64 = (Elf64_Addr *)(target->sh_addr + rel->r_offset);
    Elf32_Addr *where32 = (Elf32_Addr *)(target->sh_addr + rel->r_offset);

    /* Obtain the symbol value. */
    Elf64_Addr val;
    status_t ret = elf_module_resolve(image, ELF64_R_SYM(rel->r_info), &val);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Perform the relocation. */
    switch (ELF64_R_TYPE(rel->r_info)) {
        case ELF_R_X86_64_NONE:
            break;
        case ELF_R_X86_64_32:
            *where32 = val + rel->r_addend;
            break;
        case ELF_R_X86_64_64:
            *where64 = val + rel->r_addend;
            break;
        case ELF_R_X86_64_PC32:
        case ELF_R_X86_64_PLT32:
            *where32 = (val + rel->r_addend) - (ptr_t)where32;
            break;
        case ELF_R_X86_64_32S:
            *where32 = val + rel->r_addend;
            break;
        default:
            kprintf(LOG_WARN, "elf: encountered unknown relocation type: %lu\n", ELF64_R_TYPE(rel->r_info));
            return STATUS_MALFORMED_IMAGE;
    }

    return STATUS_SUCCESS;
}
