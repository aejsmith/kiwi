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
#include "libkernel.h"

extern ElfW(Dyn) _DYNAMIC[];

/** Kernel library initialisation function.
 *
 * The first job of this function is to relocate the the library. The kernel
 * just loads us somewhere and does not perform any relocations. We must
 * therefore relocate ourselves before we can make any calls to exported
 * functions - this means no system calls. Since our internal functions are
 * marked as hidden, we can call these (they are not called via the PLT).
 *
 * @param args		Process argument block.
 */
void libkernel_init(process_args_t *args) {
	int i;

	/* Fill in the libkernel image structure with information we have. */
	libkernel_image.load_base = args->load_base;
	libkernel_image.dyntab = (ElfW(Dyn) *)((ElfW(Addr))_DYNAMIC + (ElfW(Addr))args->load_base);

	/* Fix up addresses in our DYNAMIC section. */
	for(i = 0; libkernel_image.dyntab[i].d_tag != ELF_DT_NULL; i++) {
		switch(libkernel_image.dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			libkernel_image.dyntab[i].d_un.d_ptr += (ElfW(Addr))args->load_base;
                        break;
                }
        }

	/* Populate the dynamic table in the image structure. */
	for(i = 0; libkernel_image.dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(libkernel_image.dyntab[i].d_tag >= ELF_DT_NUM || libkernel_image.dyntab[i].d_tag == ELF_DT_NEEDED) {
			continue;
		}
		libkernel_image.dynamic[libkernel_image.dyntab[i].d_tag] = libkernel_image.dyntab[i].d_un.d_ptr;
        }

	/* Get the architecture to relocate us. */
	libkernel_arch_init(args);

	handle_t handle;

	device_open("/kconsole", &handle);
	device_write(handle, "Hello World\n", 12, 0, NULL);
	while(1);
}
