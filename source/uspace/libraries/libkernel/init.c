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
 * @brief		Kernel library initialisation function.
 */

#include <kernel/device.h>
#include <kernel/status.h>

#include <stdio.h>

#include "libkernel.h"

extern void libkernel_init_stage2(process_args_t *args);

extern elf_dyn_t _DYNAMIC[];

/** Kernel library 1st stage initialisation.
 *
 * The job of this function is to relocate the the library. The kernel just
 * loads us somewhere and does not perform any relocations. We must therefore
 * relocate ourselves before we can make any calls to exported functions or
 * use global variables.
 *
 * @param args		Process argument block.
 */
void libkernel_init(process_args_t *args) {
	rtld_image_t *image;
	int i;

	/* Work out the correct location of the libkernel image structure and
	 * fill it in with information we have. */
	image = (rtld_image_t *)((elf_addr_t)&libkernel_image + (elf_addr_t)args->load_base);
	image->load_base = args->load_base;
	image->dyntab = (elf_dyn_t *)((elf_addr_t)_DYNAMIC + (elf_addr_t)args->load_base);

	/* Fix up addresses in our DYNAMIC section. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		switch(image->dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			image->dyntab[i].d_un.d_ptr += (elf_addr_t)args->load_base;
                        break;
                }
        }

	/* Populate the dynamic table in the image structure. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED) {
			continue;
		}
		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
        }

	/* Get the architecture to relocate us. */
	libkernel_arch_init(args, image);

	/* Jump to the second stage initialisation. Annoyingly, GCC caches the
	 * location of libkernel_image, so if we try to get at it from this
	 * function it will use the old address. So, we must continue in a
	 * separate function (global, to prevent GCC from inlining it). */
	libkernel_init_stage2(args);
}

/** Second stage initialisation.
 * @param args		Process argument block. */
void libkernel_init_stage2(process_args_t *args) {
	handle_t handle;

	/* If we're the first process, open handles to the kernel console. */
	if(process_id(-1) == 1) {
		device_open("/kconsole", &handle);
		device_open("/kconsole", &handle);
		device_open("/kconsole", &handle);
	}

	/* Initialise the heap. */
	libkernel_heap_init();

	printf("libkernel: loading program %s...\n", args->path);
	process_exit(STATUS_NOT_IMPLEMENTED);
	while(1);
}
