/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               AMD64 RTLD relocation code.
 */

#include <string.h>

#include "libkernel.h"

/** Internal part of relocation.
 * @param image         Image to relocate.
 * @param relocs        Relocation table.
 * @param size          Size of relocations.
 * @return              Status code describing result of the operation. */
static status_t do_relocations(rtld_image_t *image, elf_rela_t *relocs, size_t size) {
    elf_sym_t *symtab;
    const char *strtab, *name;
    size_t i;
    elf_addr_t *addr;
    int type, symidx, bind;
    rtld_symbol_t symbol;

    symtab = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
    strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

    for (i = 0; i < size / sizeof(elf_rela_t); i++) {
        type   = ELF64_R_TYPE(relocs[i].r_info);
        addr   = (elf_addr_t *)(image->load_base + relocs[i].r_offset);
        symidx = ELF64_R_SYM(relocs[i].r_info);
        name   = strtab + symtab[symidx].st_name;
        bind   = ELF_ST_BIND(symtab[symidx].st_info);

        symbol.addr = 0;
        symbol.image = image;

        if (symidx != 0) {
            if (bind == ELF_STB_LOCAL) {
                symbol.addr = symtab[symidx].st_value;
            } else if (!rtld_symbol_lookup(image, name, &symbol)) {
                if (bind != ELF_STB_WEAK) {
                    printf("rtld: %s: cannot resolve symbol '%s'\n", image->name, name);
                    return STATUS_MISSING_SYMBOL;
                }
            }
        }

        /* Perform the actual relocation. */
        switch (type) {
        case ELF_R_X86_64_NONE:
            break;
        case ELF_R_X86_64_64:
            *addr = symbol.addr + relocs[i].r_addend;
            break;
        case ELF_R_X86_64_PC32:
            *addr = symbol.addr + relocs[i].r_addend - relocs[i].r_offset;
            break;
        case ELF_R_X86_64_GLOB_DAT:
        case ELF_R_X86_64_JUMP_SLOT:
            *addr = symbol.addr + relocs[i].r_addend;
            break;
        case ELF_R_X86_64_RELATIVE:
            *addr = (elf_addr_t)image->load_base + relocs[i].r_addend;
            break;
        case ELF_R_X86_64_COPY:
            if (symbol.addr)
                memcpy((char *)addr, (char *)symbol.addr, symtab[symidx].st_size);

            break;
        case ELF_R_X86_64_DTPMOD64:
            *addr = image->id;
            break;
        case ELF_R_X86_64_DTPOFF64:
            *addr = symbol.addr + relocs[i].r_addend;
            break;
        case ELF_R_X86_64_TPOFF64:
            *addr = symbol.addr + symbol.image->tls_offset + relocs[i].r_addend;
            break;
        default:
            dprintf("rtld: %s: unhandled relocation type %d\n", image->name, type);
            return STATUS_NOT_SUPPORTED;
        }
    }

    return STATUS_SUCCESS;
}

/** Perform relocations for am image.
 * @param image         Image to relocate.
 * @return              Status code describing result of the operation. */
status_t arch_rtld_image_relocate(rtld_image_t *image) {
    elf_rela_t *relocs;
    status_t ret;

    /* First perform RELA relocations. */
    relocs = (elf_rela_t *)image->dynamic[ELF_DT_REL_TYPE];
    ret = do_relocations(image, relocs, image->dynamic[ELF_DT_RELSZ_TYPE]);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Then PLT relocations. */
    relocs = (elf_rela_t *)image->dynamic[ELF_DT_JMPREL];
    return do_relocations(image, relocs, image->dynamic[ELF_DT_PLTRELSZ]);
}
