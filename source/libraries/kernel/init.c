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
#include <kernel/object.h>

#include <string.h>

#include "libkernel.h"

extern void libkernel_init_stage2(process_args_t *args);
extern void kern_process_loaded(void);

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

	/* Populate the dynamic table and do address fixups. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED) {
			continue;
		}

		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;

		/* Do address fixups. */
		switch(image->dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			image->dynamic[image->dyntab[i].d_tag] += (elf_addr_t)image->load_base;
			break;
		}
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
	void (*entry)(process_args_t *);
	bool dry_run = false;
	handle_t handle;
	int i;

	/* If we're the first process, open handles to the kernel console. */
	if(kern_process_id(-1) == 1) {
		for(i = 0; i < 3; i++) {
			kern_device_open("/kconsole", DEVICE_READ, &handle);
			kern_handle_control(handle, HANDLE_SET_LFLAGS, HANDLE_INHERITABLE, NULL);
		}
	}

	/* Check if any of our options are set. */
	for(i = 0; i < args->env_count; i++) {
		if(strncmp(args->env[i], "RTLD_DRYRUN=", 12) == 0) {
			dry_run = true;
		} else if(strncmp(args->env[i], "LIBKERNEL_DEBUG=", 15) == 0) {
			libkernel_debug = true;
		}
	}

	/* Initialise the runtime loader and load the program. */
	entry = (void (*)(process_args_t *))rtld_init(args, dry_run);

	/* Signal to the kernel that we've completed loading and call the entry
	 * point for the program. */
	kern_process_loaded();
	dprintf("libkernel: beginning program execution at %p...\n", entry);
	entry(args);
	dprintf("libkernel: program entry point returned\n");
	kern_process_exit(0);
}
