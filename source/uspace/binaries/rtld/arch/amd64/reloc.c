/*
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
 * @brief		RTLD AMD64 relocation code.
 */

#include <kernel/errors.h>

#include "../../image.h"
#include "../../symbol.h"
#include "../../utility.h"

/** Internal part of relocation.
 * @param image		Image to relocate.
 * @param relocs	Relocation table.
 * @param size		Size of relocations.
 * @return		0 on success, negative error code on failure. */
static int rtld_image_relocate_internal(rtld_image_t *image, ElfW(ELF_REL_TYPE) *relocs, size_t size) {
	ElfW(Addr) *addr, sym_addr;
	const char *strtab, *name;
	int type, symidx, bind;
	ElfW(Sym) *symtab;
	size_t i;

	symtab = (ElfW(Sym) *)image->dynamic[ELF_DT_SYMTAB];
	strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

	/* First perform RELA relocations. */
	for(i = 0; i < size / sizeof(ElfW(ELF_REL_TYPE)); i++) {
		type   = ELF64_R_TYPE(relocs[i].r_info);
		addr   = (ElfW(Addr) *)(image->load_base + relocs[i].r_offset);
		symidx = ELF64_R_SYM(relocs[i].r_info);
		name   = strtab + symtab[symidx].st_name;
		bind   = ELF_ST_BIND(symtab[symidx].st_info);
		sym_addr = 0;

		if(symidx != 0) {
			if(!rtld_symbol_lookup(image, name, &sym_addr) && bind != ELF_STB_WEAK) {
				printf("RTLD: Cannot resolve symbol %s in %s\n", name, image->name);
				return -ERR_FORMAT_INVAL;
			}
		}

		/* Perform the actual relocation. */
		switch(type) {
		case ELF_R_X86_64_NONE:
			break;
		case ELF_R_X86_64_64:
			*addr = sym_addr + relocs[i].r_addend;
			break;
		case ELF_R_X86_64_PC32:
			*addr = sym_addr + relocs[i].r_addend - relocs[i].r_offset;
			break;
		case ELF_R_X86_64_GLOB_DAT:
		case ELF_R_X86_64_JUMP_SLOT:
			*addr = sym_addr + relocs[i].r_addend;
			break;
		case ELF_R_X86_64_RELATIVE:
			*addr = (ElfW(Addr))image->load_base + relocs[i].r_addend;
			break;
		case ELF_R_X86_64_COPY:
			if(sym_addr) {
				memcpy((char *)addr, (char *)sym_addr, symtab[symidx].st_size);
			}
			break;
		default:
			dprintf("RTLD: Unhandled relocation type %u for %s!\n", type, image->name);
			return -ERR_NOT_SUPPORTED;
		}
	}

	return 0;
}

/** Perform relocations for am image.
 * @param image		Image to relocate.
 * @return		0 on success, negative error code on failure. */
int rtld_image_relocate(rtld_image_t *image) {
	ElfW(ELF_REL_TYPE) *relocs;
	int ret;

	/* First perform RELA relocations. */
	relocs = (ElfW(ELF_REL_TYPE) *)image->dynamic[ELF_DT_REL_TYPE];
	if((ret = rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_RELSZ_TYPE])) != 0) {
		return ret;
	}

	/* Then PLT relocations. */
	relocs = (ElfW(ELF_REL_TYPE) *)image->dynamic[ELF_DT_JMPREL];
	if((ret = rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_PLTRELSZ])) != 0) {
		return ret;
	}

	return 0;
}
