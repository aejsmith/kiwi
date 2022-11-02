/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               RTLD symbol functions.
 */

#include <string.h>

#include "libkernel.h"

/** Work out the ELF hash for a symbol name.
 * @param name          Name to get hash of.
 * @return              Hash of symbol name. */
static unsigned long hash_symbol(const unsigned char *name) {
    unsigned long h = 0, g;

    while (*name) {
        h = (h << 4) + *name++;
        if ((g = h & 0xf0000000))
            h ^= g >> 24;
        h &= ~g;
    }

    return h;
}

/** Looks up a symbol in all loaded images.
 * @param name          Name of symbol to look up.
 * @param flags         Behaviour flags (SYMBOL_LOOKUP_*).
 * @param symbol        Structure to fill in with symbol information.
 * @return              Whether the symbol was found. */
bool rtld_symbol_lookup(const char *name, uint32_t flags, rtld_symbol_t *symbol) {
    unsigned long hash = hash_symbol((const unsigned char *)name);

    core_list_foreach(&loaded_images, iter) {
        rtld_image_t *image = core_list_entry(iter, rtld_image_t, header);

        if (flags & SYMBOL_LOOKUP_EXCLUDE_APP && image == application_image)
            continue;

        /* If the hash table is empty we do not need to do anything. */
        if (image->h_nbucket == 0)
            continue;

        elf_sym_t *symtab  = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
        const char *strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

        /* Loop through all hash table entries. */
        for (Elf32_Word i = image->h_buckets[hash % image->h_nbucket];
             i != ELF_STN_UNDEF;
             i = image->h_chains[i])
        {
            uint8_t type = ELF_ST_TYPE(symtab[i].st_info);

            if ((symtab[i].st_value == 0 && type != ELF_STT_TLS) ||
                (symtab[i].st_shndx == ELF_SHN_UNDEF) ||
                (type > ELF_STT_FUNC && type != ELF_STT_COMMON && type != ELF_STT_TLS) ||
                (strcmp(strtab + symtab[i].st_name, name) != 0))
            {
                continue;
            }

            if (ELF_ST_TYPE(symtab[i].st_info) == ELF_STT_TLS) {
                symbol->addr = symtab[i].st_value;
            } else {
                /* Cannot look up non-global symbols. */
                if (ELF_ST_BIND(symtab[i].st_info) != ELF_STB_GLOBAL &&
                    ELF_ST_BIND(symtab[i].st_info) != ELF_STB_WEAK)
                {
                    break;
                }

                symbol->addr = (elf_addr_t)image->load_base + symtab[i].st_value;
            }

            symbol->image = image;
            return true;
        }
    }

    return false;
}

/** Initialise symbol fields in an image.
 * @param image         Image to initialise. */
void rtld_symbol_init(rtld_image_t *image) {
    if (image->dynamic[ELF_DT_HASH]) {
        Elf32_Word *addr = (Elf32_Word *)image->dynamic[ELF_DT_HASH];

        image->h_nbucket = *addr++;
        image->h_nchain  = *addr++;
        image->h_buckets = addr;

        addr += image->h_nbucket;

        image->h_chains = addr;
    }
}
