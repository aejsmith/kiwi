/* Kiwi kernel heap manager
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Kernel heap manager.
 */

#ifndef __MM_KHEAP_H
#define __MM_KHEAP_H

#include <arch/page.h>

#include <mm/vmem.h>

#include <types.h>

extern vmem_t kheap_raw_arena;
extern vmem_t kheap_va_arena;
extern vmem_t kheap_arena;

extern vmem_resource_t kheap_anon_afunc(vmem_t *source, vmem_resource_t size, int vmflag);
extern void kheap_anon_ffunc(vmem_t *source, vmem_resource_t addr, vmem_resource_t size);

extern void *kheap_alloc(size_t size, int vmflag);
extern void kheap_free(void *addr, size_t size);

extern void *kheap_map_range(phys_ptr_t base, size_t size, int vmflag);
extern void kheap_unmap_range(void *addr, size_t size);

extern void kheap_early_init(void);
extern void kheap_init(void);

#endif /* __MM_KHEAP_H */
