/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		IA32 kernel library initialisation function.
 */

#include "../../libkernel.h"

/** Kernel library architecture initialisation function.
 * @param args		Process argument block.
 * @param image		Kernel library image structure. */
void libkernel_arch_init(process_args_t *args, rtld_image_t *image) {
	elf_rel_t *relocs;
	elf_addr_t *addr;
	size_t count, i;

	count = image->dynamic[ELF_DT_RELSZ_TYPE] / sizeof(elf_rel_t);
	relocs = (elf_rel_t *)image->dynamic[ELF_DT_REL_TYPE];
	for(i = 0; i < count; i++) {
		addr = (elf_addr_t *)(image->load_base + relocs[i].r_offset);

		switch(ELF32_R_TYPE(relocs[i].r_info)) {
		case ELF_R_386_RELATIVE:
			*addr += (elf_addr_t)image->load_base;
			break;
		case ELF_R_386_TLS_DTPMOD32:
			*addr = LIBKERNEL_TLS_ID;
			break;
		default:
			kern_process_exit(STATUS_MALFORMED_IMAGE);
			break;
		}
	}
}
