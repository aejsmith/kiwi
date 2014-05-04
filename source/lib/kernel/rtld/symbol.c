/*
 * Copyright (C) 2009-2014 Alex Smith
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
 * @brief		RTLD symbol functions.
 */

#include <string.h>

#include "libkernel.h"

/** Work out the ELF hash for a symbol name.
 * @param name		Name to get hash of.
 * @return		Hash of symbol name. */
static unsigned long hash_symbol(const unsigned char *name) {
	unsigned long h = 0, g;

	while(*name) {
		h = (h << 4) + *name++;
		if((g = h & 0xf0000000))
			h ^= g >> 24;
		h &= ~g;
	}

	return h;
}

/**
 * Look up a symbol.
 *
 * Looks up a symbol in all loaded images. Assumes that the image that the
 * symbol is being looked up for is in the loaded images list.
 *
 * @param start		Image to look up symbol for.
 * @param name		Name of symbol to look up.
 * @param symbol	Structure to fill in with symbol information.
 *
 * @return		Whether the symbol was found.
 */
bool rtld_symbol_lookup(rtld_image_t *start, const char *name, rtld_symbol_t *symbol) {
	unsigned long hash;
	sys_list_t *iter;
	rtld_image_t *image;
	elf_sym_t *symtab;
	const char *strtab;
	Elf32_Word i;
	uint8_t type;

	hash = hash_symbol((const unsigned char *)name);

	/* Iterate through all images, starting at the image after the image
	 * that requires the symbol. */
	iter = start->header.next;
	do {
		if(iter == &loaded_images) {
			iter = iter->next;
			continue;
		}

		image = sys_list_entry(iter, rtld_image_t, header);
		iter = iter->next;

		/* If the hash table is empty we do not need to do anything. */
		if(image->h_nbucket == 0)
			continue;

		symtab = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
		strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

		/* Loop through all hash table entries. */
		for(i = image->h_buckets[hash % image->h_nbucket];
			i != ELF_STN_UNDEF;
			i = image->h_chains[i])
		{
			type = ELF_ST_TYPE(symtab[i].st_info);

			if((symtab[i].st_value == 0 && type != ELF_STT_TLS)
				|| (symtab[i].st_shndx == ELF_SHN_UNDEF)
				|| (type > ELF_STT_FUNC && type != ELF_STT_COMMON && type != ELF_STT_TLS)
				|| (strcmp(strtab + symtab[i].st_name, name) != 0))
			{
				continue;
			}

			if(ELF_ST_TYPE(symtab[i].st_info) == ELF_STT_TLS) {
				symbol->addr = symtab[i].st_value;
			} else {
				/* Cannot look up non-global symbols. */
				if(ELF_ST_BIND(symtab[i].st_info) != ELF_STB_GLOBAL
					&& ELF_ST_BIND(symtab[i].st_info) != ELF_STB_WEAK)
				{
					break;
				}

				symbol->addr = (elf_addr_t)image->load_base + symtab[i].st_value;
			}

			symbol->image = image;
			return true;
		}
	} while(iter != start->header.next);

	return false;
}

/** Initialise symbol fields in an image.
 * @param image		Image to initialise. */
void rtld_symbol_init(rtld_image_t *image) {
	Elf32_Word *addr;

	if(image->dynamic[ELF_DT_HASH]) {
		addr = (Elf32_Word *)image->dynamic[ELF_DT_HASH];
		image->h_nbucket = *addr++;
		image->h_nchain  = *addr++;
		image->h_buckets = addr;
		addr += image->h_nbucket;
		image->h_chains = addr;
	}
}
