/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		AMD64 RTLD relocation code.
 */

#include <string.h>
#include "../../libkernel.h"

/** Internal part of relocation.
 * @param image		Image to relocate.
 * @param relocs	Relocation table.
 * @param size		Size of relocations.
 * @return		Status code describing result of the operation. */
static status_t rtld_image_relocate_internal(rtld_image_t *image, elf_rela_t *relocs, size_t size) {
	elf_addr_t *addr, sym_addr;
	const char *strtab, *name;
	int type, symidx, bind;
	rtld_image_t *source;
	elf_sym_t *symtab;
	size_t i;

	symtab = (elf_sym_t *)image->dynamic[ELF_DT_SYMTAB];
	strtab = (const char *)image->dynamic[ELF_DT_STRTAB];

	for(i = 0; i < size / sizeof(elf_rela_t); i++) {
		type   = ELF64_R_TYPE(relocs[i].r_info);
		addr   = (elf_addr_t *)(image->load_base + relocs[i].r_offset);
		symidx = ELF64_R_SYM(relocs[i].r_info);
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
			*addr = (elf_addr_t)image->load_base + relocs[i].r_addend;
			break;
		case ELF_R_X86_64_COPY:
			if(sym_addr) {
				memcpy((char *)addr, (char *)sym_addr, symtab[symidx].st_size);
			}
			break;
		case ELF_R_X86_64_DTPMOD64:
			*addr = image->tls_module_id;
			break;
		case ELF_R_X86_64_DTPOFF64:
			*addr = sym_addr + relocs[i].r_addend;
			break;
		case ELF_R_X86_64_TPOFF64:
			*addr = sym_addr + source->tls_offset + relocs[i].r_addend;
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
	elf_rela_t *relocs;
	status_t ret;

	/* First perform RELA relocations. */
	relocs = (elf_rela_t *)image->dynamic[ELF_DT_REL_TYPE];
	ret = rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_RELSZ_TYPE]);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Then PLT relocations. */
	relocs = (elf_rela_t *)image->dynamic[ELF_DT_JMPREL];
	return rtld_image_relocate_internal(image, relocs, image->dynamic[ELF_DT_PLTRELSZ]);
}
