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
#include <kernel/system.h>

#include <elf.h>
#include <string.h>

#include "libkernel.h"

/** System page size. */
size_t page_size;

/** Kernel library main function.
 * @param args		Process argument block. */
void libkernel_init(process_args_t *args) {
	rtld_image_t *image;
	elf_ehdr_t *ehdr;
	elf_phdr_t *phdrs;
	handle_t handle;
	bool dry_run = false;
	void (*entry)(process_args_t *);
	void (*func)(void);
	size_t i;
	status_t ret;

	/* Fill in the libkernel image structure with information we have. */
	image = &libkernel_image;
	image->load_base = args->load_base;
	image->dyntab = _DYNAMIC;

	/* Populate the dynamic table and do address fixups. */
	for(i = 0; image->dyntab[i].d_tag != ELF_DT_NULL; i++) {
		if(image->dyntab[i].d_tag >= ELF_DT_NUM) {
			continue;
		} else if(image->dyntab[i].d_tag == ELF_DT_NEEDED) {
			continue;
		}

		/* Do address fixups. */
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

		image->dynamic[image->dyntab[i].d_tag] = image->dyntab[i].d_un.d_ptr;
	}

	/* Find out where our TLS segment is loaded to. */
	ehdr = (elf_ehdr_t *)args->load_base;
	phdrs = (elf_phdr_t *)(args->load_base + ehdr->e_phoff);
	for(i = 0; i < ehdr->e_phnum; i++) {
		if(phdrs[i].p_type == ELF_PT_TLS) {
			if(!phdrs[i].p_memsz)
				break;

			image->tls_module_id = tls_alloc_module_id();
			image->tls_image = args->load_base + phdrs[i].p_vaddr;
			image->tls_filesz = phdrs[i].p_filesz;
			image->tls_memsz = phdrs[i].p_memsz;
			image->tls_align = phdrs[i].p_align;
			break;
		}
	}

	/* Get the system page size. */
	kern_system_info(SYSTEM_INFO_PAGE_SIZE, &page_size);

	/* Save the current process ID for the kern_process_id() wrapper. */
	curr_process_id = _kern_process_id(PROCESS_SELF);

	/* Let the kernel know where kern_thread_restore() is. */
	kern_process_control(PROCESS_SET_RESTORE, kern_thread_restore, NULL);

	/* If we're the first process, open handles to the kernel console. */
	if(curr_process_id == 1) {
		kern_device_open("/kconsole", FILE_ACCESS_READ, 0, &handle);
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);
		kern_device_open("/kconsole", FILE_ACCESS_WRITE, 0, &handle);
		kern_handle_set_flags(handle, HANDLE_INHERITABLE);
		kern_device_open("/kconsole", FILE_ACCESS_WRITE, 0, &handle);
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
	if(image->tls_module_id)
		image->tls_offset = tls_tp_offset(image);
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
	exception_info_t info;

	info.code = EXCEPTION_ABORT;
	kern_thread_raise(&info);
	kern_thread_exit(0);
}
