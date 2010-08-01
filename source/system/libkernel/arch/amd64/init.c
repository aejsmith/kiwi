/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		AMD64 kernel library initialisation function.
 */

#include "../../libkernel.h"

/** Kernel library architecture initialisation function.
 * @param args		Process argument block.
 * @param image		Kernel library image structure. */
void libkernel_arch_init(process_args_t *args, rtld_image_t *image) {
	elf_rela_t *relocs;
	elf_addr_t *addr;
	size_t count, i;

	/* First perform RELA relocations. */
	count = image->dynamic[ELF_DT_RELSZ_TYPE] / sizeof(elf_rela_t);
	relocs = (elf_rela_t *)image->dynamic[ELF_DT_REL_TYPE];
	for(i = 0; i < count; i++) {
		if(ELF64_R_TYPE(relocs[i].r_info) != ELF_R_X86_64_RELATIVE) {
			continue;
		}

		addr = (elf_addr_t *)(image->load_base + relocs[i].r_offset);
		*addr = (elf_addr_t)image->load_base + relocs[i].r_addend;
	}
}
