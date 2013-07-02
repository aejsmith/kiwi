/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		MMU interface.
 */

#include <arch/memory.h>

#include <mm/mmu.h>

#include <kboot.h>

/** Initialize the kernel MMU context. */
__init_text void mmu_init(void) {
	arch_mmu_init();
}

/** Perform per-CPU MMU initialization. */
__init_text void mmu_init_percpu(void) {
	ptr_t start, i;
	size_t size;
	phys_ptr_t phys;

	/* Do architecture-specific initialization. */
	arch_mmu_init_percpu();

	mmu_context_lock(&kernel_mmu_context);

	/* Duplicate all virtual memory mappings created by KBoot. */
	KBOOT_ITERATE(KBOOT_TAG_VMEM, kboot_tag_vmem_t, range) {
		start = range->start;
		size = range->size;
		phys = range->phys;

		/* Only want to map ranges in kmem space, and non-special
		 * mappings. */
		if(start < KERNEL_KMEM_BASE || start + size > KERNEL_KMEM_BASE + KERNEL_KMEM_SIZE) {
			continue;
		} else if(phys == ~((uint64_t)0)) {
			continue;
		}

		for(i = 0; i < size; i += PAGE_SIZE) {
			mmu_context_map(&kernel_mmu_context, start + i, phys + i,
				true, false, MM_BOOT);
		}
	}

	mmu_context_unlock(&kernel_mmu_context);

	/* Switch to the kernel context. */
	mmu_context_switch(&kernel_mmu_context);
}
