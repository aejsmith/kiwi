/* Kiwi AMD64 module loading functions
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
 * @brief		AMD64 module loading functions.
 */

#include <arch/memmap.h>

#include <console/kprintf.h>

#include <mm/kheap.h>
#include <mm/vmem.h>

#include <module.h>

#if CONFIG_MODULE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void *module_mem_alloc(size_t size, int mmflag);
extern void module_mem_free(void *base, size_t size);

/** Arenas used for allocating memory for kernel modules. */
static vmem_t *module_raw_arena;
static vmem_t *module_arena;

/** Allocate memory suitable to hold a kernel module.
 * @param size		Size of the allocation.
 * @param mmflag	Allocation flags.
 * @return		Address allocated or NULL if no available memory. */
void *module_mem_alloc(size_t size, int mmflag) {
	/* Create the arenas if they have not been created. */
	if(!module_raw_arena) {
		module_raw_arena = vmem_create("module_raw_arena", KERNEL_MODULE_BASE,
		                               KERNEL_MODULE_SIZE, PAGE_SIZE, NULL,
		                               NULL, NULL, 0, MM_FATAL);
		module_arena = vmem_create("module_arena", NULL, 0, PAGE_SIZE,
		                           kheap_anon_afunc, kheap_anon_ffunc,
		                           module_raw_arena, 0, MM_FATAL);
	}

	return (void *)((ptr_t)vmem_alloc(module_arena, size, mmflag));
}

/** Free memory holding a module.
 * @param base		Base of the allocation.
 * @param size		Size of the allocation. */
void module_mem_free(void *base, size_t size) {
	vmem_free(module_arena, (vmem_resource_t)((ptr_t)base), size);
}
