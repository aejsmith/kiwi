/* Kiwi IA32 module loading functions
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
 * @brief		IA32 module loading functions.
 */

#include <console/kprintf.h>

#include <lib/utility.h>

#include <errors.h>
#include <module.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern int module_elf_relocate(module_t *module, void *image, size_t size, bool external,
                               int (*get_sym)(module_t *, size_t, bool, elf_addr_t *));

/** Perform relocations for an ELF module.
 * @param module	Module to relocate.
 * @param image		Module image read from disk.
 * @param size		Size of the module image.
 * @param external	Whether to handle external or internal symbols.
 * @param get_sym	Helper function to do symbol lookup.
 * @return		0 on success, negative error code on failure. */
int module_elf_relocate(module_t *module, void *image, size_t size, bool external,
                        int (*get_sym)(module_t *, size_t, bool, elf_addr_t *)) {
	Elf32_Addr *where32, val = 0;
	Elf32_Shdr *sect, *targ;
	Elf32_Rel *rel;
	size_t i, r;
	int ret;

	/* Look for relocation sections in the module. */
	for(i = 0; i < module->ehdr.e_shnum; i++) {
		sect = MODULE_ELF_SECT(module, i);
		if(sect->sh_type != ELF_SHT_REL && sect->sh_type != ELF_SHT_RELA) {
			continue;
		}

		/* Get the relocation target section. */
		targ = MODULE_ELF_SECT(module, sect->sh_info);

		/* Loop through all the relocations. */
		for(r = 0; r < (sect->sh_size / sect->sh_entsize); r++) {
			rel = (Elf32_Rel *)(image + sect->sh_offset + (r * sect->sh_entsize));

			/* Get the location of the relocation. */
			where32 = (Elf32_Addr *)(targ->sh_addr + rel->r_offset);

			ret = get_sym(module, ELF32_R_SYM(rel->r_info), external, &val);
			if(ret < 0) {
				return ret;
			} else if(ret == 0) {
				continue;
			}

			/* Perform the actual relocation. */
			switch(ELF32_R_TYPE(rel->r_info)) {
			case ELF_R_386_NONE:
				break;
			case ELF_R_386_32:
				*where32 = val + *where32;
				break;
			case ELF_R_386_PC32:
				*where32 = val + *where32 - (uint32_t)where32;
				break;
			default:
				dprintf("module: encountered unknown relocation type: %lu\n",
				        ELF32_R_TYPE(rel->r_info));
				return -ERR_FORMAT_INVAL;
			}
		}
	}

	return 0;
}
