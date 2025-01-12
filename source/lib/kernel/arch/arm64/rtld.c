/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 RTLD relocation code.
 */

#include <string.h>

#include "libkernel.h"


/** Internal part of relocation.
 * @param image         Image to relocate.
 * @param relocs        Relocation table.
 * @param size          Size of relocations.
 * @return              Status code describing result of the operation. */
static status_t do_relocations(rtld_image_t *image, elf_rela_t *relocs, size_t size) {
    elf_sym_t *symtab  = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
    const char *strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

    for (size_t i = 0; i < size / sizeof(elf_rela_t); i++) {
        int type         = ELF64_R_TYPE(relocs[i].r_info);
        int sym_idx      = ELF64_R_SYM(relocs[i].r_info);
        const char *name = strtab + symtab[sym_idx].st_name;
        int bind         = ELF_ST_BIND(symtab[sym_idx].st_info);
        elf_addr_t *addr = (elf_addr_t *)(image->load_base + relocs[i].r_offset);

        rtld_symbol_t symbol;
        symbol.addr  = 0;
        symbol.image = image;

        if (sym_idx != 0) {
            if (bind == ELF_STB_LOCAL) {
                symbol.addr = symtab[sym_idx].st_value;
            } else {
                uint32_t flags = 0;

                /* COPY relocations should not resolve to the app image's own
                 * symbol, we want to copy it from a library. */
                if (type == ELF_R_AARCH64_COPY)
                    flags |= SYMBOL_LOOKUP_EXCLUDE_APP;

                if (!rtld_symbol_lookup(name, flags, &symbol)) {
                    if (bind != ELF_STB_WEAK) {
                        printf("rtld: %s: cannot resolve symbol '%s'\n", image->name, name);
                        return STATUS_MISSING_SYMBOL;
                    }
                }
            }
        }

        /* Perform the actual relocation. */
        switch (type) {
            case ELF_R_AARCH64_NONE:
                break;
            case ELF_R_AARCH64_ABS64:
            case ELF_R_AARCH64_GLOB_DAT:
            case ELF_R_AARCH64_JUMP_SLOT:
                *addr = symbol.addr + relocs[i].r_addend;
                break;
            case ELF_R_AARCH64_RELATIVE:
                *addr = (elf_addr_t)image->load_base + relocs[i].r_addend;
                break;
            case ELF_R_AARCH64_COPY:
                if (symbol.addr)
                    memcpy((char *)addr, (char *)symbol.addr, symtab[sym_idx].st_size);

                break;
            case ELF_R_AARCH64_TLSDESC:
// TODO  
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
    status_t ret;

    /* First perform RELA relocations. */
    elf_rela_t *rela = (elf_rela_t *)image->dynamic[ELF_DT_REL_TYPE];
    ret = do_relocations(image, rela, image->dynamic[ELF_DT_RELSZ_TYPE]);
    if (ret != STATUS_SUCCESS)
        return ret;

    /* Then PLT relocations. */
    elf_rela_t *plt = (elf_rela_t *)image->dynamic[ELF_DT_JMPREL];
    return do_relocations(image, plt, image->dynamic[ELF_DT_PLTRELSZ]);
}
