/* Kiwi RTLD symbol functions
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		RTLD symbol functions.
 */

#include <rtld/export.h>
#include <rtld/symbol.h>
#include <rtld/utility.h>

/** Work out the ELF hash for a symbol name.
 * @param name		Name to get hash of.
 * @return		Hash of symbol name. */
static unsigned long rtld_symbol_hash(const unsigned char *name) {
	unsigned long h = 0, g;

	while(*name) {
		h = (h << 4) + *name++;
		if((g = h & 0xf0000000)) {
			h ^= g >> 24;
		}
		h &= ~g;
	}
	return h;
}

/** Look up a symbol.
 * @note		Assumes that the image is in the list.
 * @param start		Image to look up symbol for.
 * @param name		Name of symbol to look up.
 * @param addrp		Where to store address of symbol.
 * @return		True if found, false if not. */
bool rtld_symbol_lookup(rtld_image_t *start, const char *name, ElfW(Addr) *addrp) {
	rtld_image_t *image;
	unsigned long hash;
	const char *strtab;
	ElfW(Sym) *symtab;
	Elf32_Word i;
	list_t *iter;

	/* Check if it matches an exported symbol. */
	for(i = 0; i < RTLD_EXPORT_COUNT; i++) {
		if(strcmp(rtld_exported_funcs[i].name, name) == 0) {
			*addrp = (ElfW(Addr))rtld_exported_funcs[i].addr;
			return true;
		}
	}

	hash = rtld_symbol_hash((const unsigned char *)name);

	/* Iterate through all images, starting at the image after the image
	 * that requires the symbol. */
	iter = start->header.next;
	do {
		if(iter == &rtld_loaded_images) {
			iter = iter->next;
			continue;
		}

		image = list_entry(iter, rtld_image_t, header);
		iter = iter->next;

		/* If the hash table is empty we do not need to do anything. */
		if(image->h_nbucket == 0) {
			continue;
		}

		symtab = (ElfW(Sym) *)image->dynamic[ELF_DT_SYMTAB];
		strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

		/* Loop through all hash table entries. */
		for(i = image->h_buckets[hash % image->h_nbucket]; i != ELF_STN_UNDEF; i = image->h_chains[i]) {
			if(symtab[i].st_shndx == ELF_SHN_UNDEF || symtab[i].st_value == 0) {
				continue;
			} else if(ELF_ST_TYPE(symtab[i].st_info) > ELF_STT_FUNC && ELF_ST_TYPE(symtab[i].st_info) != ELF_STT_COMMON) {
				continue;
			} else if(strcmp(strtab + symtab[i].st_name, name) != 0) {
				continue;
			}

			/* Cannot look up non-global symbols. */
			if(ELF_ST_BIND(symtab[i].st_info) != ELF_STB_GLOBAL && ELF_ST_BIND(symtab[i].st_info) != ELF_STB_WEAK) {
				break;
			}

			*addrp = (ElfW(Addr))image->load_base + symtab[i].st_value;
			return true;
		}
	} while(iter != start->header.next);

	return false;
}
