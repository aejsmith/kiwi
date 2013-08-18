/*
 * Copyright (C) 2010-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel library initialisation function.
 */

#include <kernel/device.h>
#include <kernel/private/process.h>
#include <kernel/private/thread.h>
#include <kernel/object.h>
#include <kernel/signal.h>

#include <elf.h>
#include <string.h>

#include "libkernel.h"

extern void libkernel_init_stage2(process_args_t *args, void *load_base);

/**
 * Kernel library 1st stage initialisation.
 *
 * The job of this function is to relocate the the library. The kernel just
 * loads us somewhere and does not perform any relocations. We must therefore
 * relocate ourselves before we can make any calls to exported functions or
 * use global variables.
 *
 * @param args		Process argument block.
 * @param load_base	Load base address.
 */
void libkernel_init(process_args_t *args, void *load_base) {
	rtld_image_t *image;
	int i;

	/* Work out the correct location of the libkernel image structure and
	 * fill it in with information we have. */
	image = (rtld_image_t *)((elf_addr_t)&libkernel_image + (elf_addr_t)load_base);
	image->load_base = load_base;
	image->dyntab = (elf_dyn_t *)((elf_addr_t)_DYNAMIC + (elf_addr_t)load_base);

	/* Populate the dynamic table and do address fixups. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM || image->dyntab[i].d_tag == ELF_DT_NEEDED)
			continue;

		/* Do address fixups. */
		switch(image->dyntab[i].d_tag) {
		case ELF_DT_HASH:
		case ELF_DT_PLTGOT:
		case ELF_DT_STRTAB:
		case ELF_DT_SYMTAB:
		case ELF_DT_JMPREL:
		case ELF_DT_REL_TYPE:
			image->dyntab[i].d_un.d_ptr += (elf_addr_t)image->load_base;
			break;
		}

		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
	}

	/* Get the architecture to relocate us. */
	libkernel_arch_init(args, image);

	/* Jump to the second stage initialisation. Annoyingly, the compiler
	 * can cache the location of libkernel_image, so if we try to get at it
	 * from this function it will use the old address. So, we must continue
	 * in a separate function (global, to prevent the compiler from inlining
	 * it). */
	libkernel_init_stage2(args, load_base);
}

/** Second stage initialisation.
 * @param args		Process argument block.
 * @param load_base	Load base address. */
void libkernel_init_stage2(process_args_t *args, void *load_base) {
	void (*entry)(process_args_t *);
	bool dry_run = false;
	rtld_image_t *image;
	void (*func)(void);
	elf_phdr_t *phdrs;
	elf_ehdr_t *ehdr;
	handle_t handle;
	size_t i;
	status_t ret;

	/* Find out where our TLS segment is loaded to. */
	ehdr = (elf_ehdr_t *)load_base;
	phdrs = (elf_phdr_t *)((elf_addr_t)load_base + ehdr->e_phoff);
	for(i = 0; i < ehdr->e_phnum; i++) {
		if(phdrs[i].p_type == ELF_PT_TLS) {
			if(!phdrs[i].p_memsz)
				break;

			libkernel_image.tls_module_id = tls_alloc_module_id();
			libkernel_image.tls_image = (void *)((elf_addr_t)load_base
				+ phdrs[i].p_vaddr);
			libkernel_image.tls_filesz = phdrs[i].p_filesz;
			libkernel_image.tls_memsz = phdrs[i].p_memsz;
			libkernel_image.tls_align = phdrs[i].p_align;
			break;
		}
	}

	/* Save the current process ID for the kern_process_id() wrapper. */
	curr_process_id = _kern_process_id(PROCESS_SELF);

	/* If we're the first process, open handles to the kernel console. */
	if(curr_process_id == 1) {
		kern_device_open("/kconsole", FILE_RIGHT_READ, 0, &handle);
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);
		kern_device_open("/kconsole", FILE_RIGHT_WRITE, 0, &handle);
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);
		kern_device_open("/kconsole", FILE_RIGHT_WRITE, 0, &handle);
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);
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
	if(dry_run)
		kern_process_exit(STATUS_SUCCESS);

	/* Set up TLS for the current thread. */
	if(libkernel_image.tls_module_id)
		libkernel_image.tls_offset = tls_tp_offset(&libkernel_image);
	ret = tls_init();
	if(ret != STATUS_SUCCESS)
		kern_process_exit(ret);

	/* Save the current thread ID in TLS for the kern_thread_id() wrapper. */
	curr_thread_id = _kern_thread_id(THREAD_SELF);

	/* Signal to the kernel that we've completed loading. */
	kern_process_control(PROCESS_LOADED, NULL, NULL);

	/* Run INIT functions for loaded images. */
	LIST_FOREACH(&loaded_images, iter) {
		image = list_entry(iter, rtld_image_t, header);
		if(image->dynamic[ELF_DT_INIT]) {
			func = (void (*)(void))(image->load_base + image->dynamic[ELF_DT_INIT]);
			dprintf("rtld: %s: calling INIT function %p...\n", image->name, func);
			func();
		}
	}

	/* Call the entry point for the program. */
	dprintf("libkernel: beginning program execution at %p...\n", entry);
	entry(args);
	dprintf("libkernel: program entry point returned\n");
	kern_process_exit(0);
}

/** Abort the process. */
void libkernel_abort(void) {
	/* Try to raise a SIGABRT, failing that (signal may be blocked) then
	 * just exit. */
	kern_signal_send(PROCESS_SELF, SIGABRT);
	kern_process_exit(-1);
}
