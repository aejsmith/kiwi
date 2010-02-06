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
 * @brief		AMD64 ELF helper functions.
 */

#include <console/kprintf.h>

#include <lib/utility.h>

#include <elf.h>
#include <errors.h>
#include <module.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern int elf_module_get_sym(module_t *module, size_t num, bool external, elf_addr_t *valp);

/** Perform relocations for an ELF module.
 * @param module	Module to relocate.
 * @param external	Whether to handle external or internal symbols.
 * @return		0 on success, negative error code on failure. */
int elf_module_relocate(module_t *module, bool external) {
	Elf64_Addr *where64, val = 0;
	Elf64_Shdr *sect, *targ;
	Elf32_Addr *where32;
	size_t i, r, bytes;
	offset_t offset;
	Elf64_Rela rel;
	int ret;

	/* Look for relocation sections in the module. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);
		if(sect->sh_type != ELF_SHT_RELA) {
			if(sect->sh_type == ELF_SHT_REL) {
				dprintf("elf: ELF_SHT_REL relocation section unsupported\n");
				return -ERR_NOT_IMPLEMENTED;
			}
			continue;
		}

		/* Get the relocation target section. */
		targ = MODULE_ELF_SECT(module, sect->sh_info);

		/* Loop through all the relocations. */
		for(r = 0; r < (sect->sh_size / sect->sh_entsize); r++) {
			offset = (offset_t)sect->sh_offset + (r * sect->sh_entsize);
			if((ret = vfs_file_read(module->node, &rel, sizeof(Elf64_Rela), offset, &bytes)) != 0) {
				return ret;
			} else if(bytes != sizeof(Elf64_Rela)) {
				return -ERR_FORMAT_INVAL;
			}

			/* Get the location of the relocation. */
			where64 = (Elf64_Addr *)(targ->sh_addr + rel.r_offset);
			where32 = (Elf32_Addr *)(targ->sh_addr + rel.r_offset);

			ret = elf_module_get_sym(module, ELF64_R_SYM(rel.r_info), external, &val);
			if(ret < 0) {
				return ret;
			} else if(ret == 0) {
				continue;
			}

			/* Perform the actual relocation. */
			switch(ELF64_R_TYPE(rel.r_info)) {
			case ELF_R_X86_64_NONE:
				break;
			case ELF_R_X86_64_32:
				*where32 = val + rel.r_addend;
				break;
			case ELF_R_X86_64_64:
				*where64 = val + rel.r_addend;
				break;
			case ELF_R_X86_64_PC32:
				*where32 = (val + rel.r_addend) - (ptr_t)where32;
				break;
			case ELF_R_X86_64_32S:
				*where32 = val + rel.r_addend;
				break;
			default:
				dprintf("module: encountered unknown relocation type: %lu\n",
				        ELF64_R_TYPE(rel.r_info));
				return -ERR_FORMAT_INVAL;
			}
		}
	}

	return 0;
}
