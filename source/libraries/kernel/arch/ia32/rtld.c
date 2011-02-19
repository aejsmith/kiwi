/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 RTLD relocation code.
 */

#include <string.h>
#include "../../libkernel.h"

/** Internal part of relocation.
 * @param image		Image to relocate.
 * @param relocs	Relocation table.
 * @param size		Size of relocations.
 * @return		Status code describing result of the operation. */
static status_t rtld_image_relocate_internal(rtld_image_t *image, elf_rel_t *relocs, size_t size) {
	elf_addr_t *addr, sym_addr;
	const char *strtab, *name;
	int type, symidx, bind;
	rtld_image_t *source;
	elf_sym_t *symtab;
	size_t i;

	symtab = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
	strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

	for(i = 0; i < size / sizeof(elf_rel_t); i++) {
		type   = ELF32_R_TYPE(relocs[i].r_info);
		addr   = (elf_addr_t *)(image->load_base + relocs[i].r_offset);
		symidx = ELF32_R_SYM(relocs[i].r_info);
		name   = strtab + symtab[symidx].st_name;
		bind   = ELF_ST_BIND(symtab[symidx].st_info);
		sym_addr = 0;
		source = image;

		if(symidx != 0) {
			if(bind == ELF_STB_LOCAL) {
				sym_addr = symtab[symidx].st_value;
			} else if(!rtld_symbol_lookup(image, name, &sym_addr, &source)) {
				if(bind != ELF_STB_WEAK) {
					printf("rtld: %s: cannot resolve symbol '%s'\n", image->name, name);
					return STATUS_MISSING_SYMBOL;
				}
			}
		}

		/* Perform the actual relocation. */
		switch(type) {
		case ELF_R_386_NONE:
			break;
		case ELF_R_386_32:
			*addr += sym_addr;
			break;
		case ELF_R_386_PC32:
			*addr += (sym_addr - (elf_addr_t)addr);
			break;
		case ELF_R_386_GLOB_DAT:
		case ELF_R_386_JMP_SLOT:
			*addr = sym_addr;
			break;
		case ELF_R_386_RELATIVE:
			*addr += (elf_addr_t)image->load_base;
			break;
		case ELF_R_386_COPY:
			if(sym_addr) {
				memcpy((char *)addr, (char *)sym_addr, symtab[symidx].st_size);
			}
			break;
		case ELF_R_386_TLS_DTPMOD32:
			*addr = source->tls_module_id;
			break;
		case ELF_R_386_TLS_DTPOFF32:
			*addr = sym_addr;
			break;
		case ELF_R_386_TLS_TPOFF32:
			*addr += (-source->tls_offset - sym_addr);
			break;
		case ELF_R_386_TLS_TPOFF:
			*addr += sym_addr + source->tls_offset;
			break;
		default:
			dprintf("rtld: %s: unhandled relocation type %d\n", image->name, type);
			return STATUS_NOT_SUPPORTED;
		}
	}

	return STATUS_SUCCESS;
}

/** Perform relocations for am image.
 * @param image		Image to relocate.
 * @return		Status code describing result of the operation. */
status_t rtld_image_relocate(rtld_image_t *image) {
	elf_rel_t *relocs;
	status_t ret;

	/* First perform RELA relocations. */
	relocs = (elf_rel_t *)image->dynamic[ELF_DT_REL_TYPE];
	ret = rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_RELSZ_TYPE]);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Then PLT relocations. */
	relocs = (elf_rel_t *)image->dynamic[ELF_DT_JMPREL];
	return rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_PLTRELSZ]);
}
