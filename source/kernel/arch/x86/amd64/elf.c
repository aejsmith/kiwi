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
 * @brief		AMD64 ELF helper functions.
 */

#include <lib/utility.h>

#include <console.h>
#include <elf.h>
#include <module.h>
#include <status.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Perform a REL relocation on an ELF module.
 * @param module	Module to relocate.
 * @param rel		Relocation to perform.
 * @param target	Section to perform relocation on.
 * @return		Status code describing result of the operation. */
status_t elf_module_apply_rel(module_t *module, elf_rel_t *rel, elf_shdr_t *target) {
	dprintf("elf: ELF_SHT_REL relocation section unsupported\n");
	return STATUS_NOT_IMPLEMENTED;
}

/** Perform a RELA relocation on an ELF module.
 * @param module	Module to relocate.
 * @param rel		Relocation to perform.
 * @param target	Section to perform relocation on.
 * @return		Status code describing result of the operation. */
status_t elf_module_apply_rela(module_t *module, elf_rela_t *rel, elf_shdr_t *target) {
	Elf64_Addr *where64, val = 0;
	Elf32_Addr *where32;
	status_t ret;

	/* Get the location of the relocation. */
	where64 = (Elf64_Addr *)(target->sh_addr + rel->r_offset);
	where32 = (Elf32_Addr *)(target->sh_addr + rel->r_offset);

	/* Obtain the symbol value. */
	ret = elf_module_lookup_symbol(module, ELF64_R_SYM(rel->r_info), &val);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Perform the relocation. */
	switch(ELF64_R_TYPE(rel->r_info)) {
	case ELF_R_X86_64_NONE:
		break;
	case ELF_R_X86_64_32:
		*where32 = val + rel->r_addend;
		break;
	case ELF_R_X86_64_64:
		*where64 = val + rel->r_addend;
		break;
	case ELF_R_X86_64_PC32:
		*where32 = (val + rel->r_addend) - (ptr_t)where32;
		break;
	case ELF_R_X86_64_32S:
		*where32 = val + rel->r_addend;
		break;
	default:
		dprintf("elf: encountered unknown relocation type: %lu\n", ELF64_R_TYPE(rel->r_info));
		return STATUS_FORMAT_INVAL;
	}

	return STATUS_SUCCESS;
}
