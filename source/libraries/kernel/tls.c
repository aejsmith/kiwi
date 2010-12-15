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
 * @brief		Thread-local storage management.
 *
 * Reference:
 *  - ELF Handling For Thread-Local Storage
 *    http://people.redhat.com/drepper/tls.pdf
 *
 * @todo		Reuse module IDs when modules are unloaded.
 * @todo		I don't think the offset calculation and alignment
 *			handling is quite right.
 * @todo		When cloning a process with kern_process_clone(), TLS
 *			data for all other threads should be freed.
 */

#include <kernel/thread.h>
#include <kernel/vm.h>

#include <stdlib.h>
#include <string.h>

#include "libkernel.h"

/** Next module ID.
 * @note		Protected by the RTLD lock - this is only ever called
 *			from the RTLD which serialises image loading. */
static size_t next_module_id = DYNAMIC_TLS_START;

/** Statically allocated DTV size. */
static ptr_t static_dtv_size = 0;

/** Look up an RTLD image by TLS module ID.
 * @param id		Module ID.
 * @return		Pointer to image. */
static rtld_image_t *tls_module_lookup(size_t id) {
	rtld_image_t *image;

	LIST_FOREACH(&loaded_images, iter) {
		image = list_entry(iter, rtld_image_t, header);

		if(image->tls_module_id == id) {
			return image;
		}
	}

	return NULL;
}

/** Allocate a new module ID.
 * @return		Allocated module ID. */
size_t tls_alloc_module_id(void) {
	return next_module_id++;
}

/** Get a TLS address.
 * @note		This is not called directly by code, it is called from
 *			an architecture-specific wrapper (__tls_get_addr on
 *			most architectures).
 * @param module	Module ID.
 * @param offset	Offset of object.
 * @return		Address of current thread's copy of the variable. */
void *tls_get_addr(size_t module, size_t offset) {
	tls_tcb_t *tcb = tls_tcb_get();
	size_t size;

	/* Check if the DTV contains this module. */
	size = (size_t)tcb->dtv[0];
	if(module >= size) {
		printf("DTV resizing is not yet implemented\n");
		return NULL;
	} else if(!tcb->dtv[module]) {
		printf("Dynamic TLS allocation not yet implemented\n");
		return NULL;
	}

	return (void *)(tcb->dtv[module] + offset);
}

#ifdef TLS_VARIANT2
/** Work out the size to allocate for the initial TLS block.
 * @return		Size to allocate. */
static size_t tls_initial_block_size(void) {
	rtld_image_t *image;
	size_t i, size = 0;

	/* tlsoffset(1) = round(tlssize(1), align(1))
	 * tlsoffset(m+1) = round(tlsoffset(m) + tlssize(m+1), align(m+1)) */
	for(i = 1; i < static_dtv_size; i++) {
		image = tls_module_lookup(i);
		if(image) {
			size = ROUND_UP(size + image->tls_memsz, image->tls_align);
		}
	}

	/* Add on the TCB size. */
	size += sizeof(tls_tcb_t);
	return size;
}

/** Initialise the initial TLS block.
 * @param base		Base address of block.
 * @param dtv		Dynamic thread vector.
 * @return		TCB pointer. */
static tls_tcb_t *tls_initial_block_init(ptr_t base, ptr_t *dtv) {
	rtld_image_t *image;
	size_t i;

	for(i = (static_dtv_size - 1); i >= 1; i--) {
		image = tls_module_lookup(i);
		if(!image) {
			continue;
		}

		/* Handle alignment requirements. */
		if(image->tls_align) {
			base = ROUND_UP(base, image->tls_align);
		}

		dprintf("tls: loading image for module %s (%zu) to %p (offset %p) for thread %d\n",
		        image->name, image->tls_module_id, base, -image->tls_offset,
		        kern_thread_id(-1));
		dtv[i] = base;

		if(image->tls_filesz) {
			memcpy((void *)base, image->tls_image, image->tls_filesz);
		}
		if(image->tls_memsz - image->tls_filesz) {
			memset((void *)(base + image->tls_filesz), 0, image->tls_memsz - image->tls_filesz);
		}
		base += image->tls_memsz;
	}

	/* Return the TCB address. */
	dprintf("tls: thread pointer for thread %d is %p\n", kern_thread_id(-1), base);
	return (void *)base;
}

/** Get the offset of an image's TLS block from the thread pointer.
 * @param image		Image to get for.
 * @return		Offset from thread pointer, or 0 if the image is not
 *			an initial image. */
ptrdiff_t tls_tp_offset(rtld_image_t *image) {
	ptrdiff_t offset = 0;
	rtld_image_t *exist;
	size_t i;

	if(static_dtv_size) {
		return 0;
	}

	/* tlsoffset(1) = round(tlssize(1), align(1))
	 * tlsoffset(m+1) = round(tlsoffset(m) + tlssize(m+1), align(m+1)) */
	for(i = 1; i < image->tls_module_id; i++) {
		exist = tls_module_lookup(i);
		if(exist) {
			offset = ROUND_UP(offset + exist->tls_memsz, exist->tls_align);
		}
	}
	offset = ROUND_UP(offset + image->tls_memsz, image->tls_align);

	/* Want the negative of what we've worked out, the data is behind the
	 * thread pointer. */
	offset = -offset;
	return offset;
}
#elif defined(TLS_VARIANT1)
# error "TLS_VARIANT1 is not implemented"
#else
# error "Please define TLS_VARIANT1 or TLS_VARIANT2"
#endif

/** Initialise TLS for the current thread.
 * @return		Status code describing result of the operation. */
status_t tls_init(void) {
	tls_tcb_t *tcb;
	status_t ret;
	size_t size;
	void *alloc;
	ptr_t *dtv;

	/* All initial modules (the executable itself and the libraries loaded
	 * along with it) must have their TLS blocks allocated statically.
	 * Any modules loaded at runtime can have their TLS blocks dynamically
	 * created. If this is the first call to this function, which will be
	 * just after loading all initial modules, we record the size we need
	 * to statically allocate. Note that the size will be:
	 *  (number of initial modules + 1)
	 * because the first DTV entry is the "generation number". This is used
	 * to record the current size of the DTV to allow it to be dynamically
	 * resized. */
	if(!static_dtv_size) {
		static_dtv_size = next_module_id;
	}

	/* Create the dynamic thread vector. */
	dtv = malloc(static_dtv_size * sizeof(ptr_t));
	if(!dtv) {
		return STATUS_NO_MEMORY;
	}

	/* Store the current size. */
	dtv[0] = static_dtv_size;

	/* Allocate the TLS block. */
	size = ROUND_UP(tls_initial_block_size(), PAGE_SIZE);
	ret = kern_vm_map(NULL, size, VM_MAP_READ | VM_MAP_WRITE | VM_MAP_PRIVATE, -1, 0, &alloc);
	if(ret != STATUS_SUCCESS) {
		free(dtv);
		return ret;
	}

	/* Initialise it and tell the kernel our TLS address. */
	tcb = tls_initial_block_init((ptr_t)alloc, dtv);
	tls_tcb_init(tcb);
	tcb->dtv = dtv;
	tcb->base = alloc;
	kern_thread_control(-1, THREAD_SET_TLS_ADDR, tcb, NULL);
	return STATUS_SUCCESS;
}

/** Destroy the TLS block for the current thread.
 * @todo		Will need to free dynamically allocated blocks here. */
void tls_destroy(void) {
	size_t size = ROUND_UP(tls_initial_block_size(), PAGE_SIZE);
	tls_tcb_t *tcb = tls_tcb_get();

	dprintf("tls: freeing block %p (size: %zu) for thread %d\n",
	        tcb->base, size, kern_thread_id(-1));
	kern_vm_unmap(tcb->base, size);
}
