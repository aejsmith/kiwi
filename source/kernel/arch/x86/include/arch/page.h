/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 paging definitions.
 */

#ifndef __ARCH_PAGE_H
#define __ARCH_PAGE_H

/** Page size definitions. */
#define PAGE_WIDTH		12		/**< Width of a page in bits. */
#define PAGE_SIZE		0x1000		/**< Size of a page (4KB). */
#define LARGE_PAGE_WIDTH	21		/**< Width of a large page in bits. */
#define LARGE_PAGE_SIZE		0x200000	/**< Size of a large page (2MB). */

/** Mask to clear page offset and unsupported bits from a virtual address. */
#ifdef __x86_64__
# define PAGE_MASK		0xFFFFFFFFFF000LL
#else
# define PAGE_MASK		0xFFFFF000
#endif

/** Mask to clear page offset and unsupported bits from a physical address. */
#ifdef __x86_64__
# define PHYS_PAGE_MASK		0xFFFFFFF000LL
#else
# define PHYS_PAGE_MASK		0xFFFFFF000LL
#endif

#if !defined(__ASM__) && !defined(LOADER)

#include <sync/mutex.h>

/** Size of TLB flush array. */
#define INVALIDATE_ARRAY_SIZE	128

/** Structure containing a hardware page map. */
typedef struct page_map {
	mutex_t lock;			/**< Lock to protect page map. */
	phys_ptr_t cr3;			/**< Value to load into the CR3 register. */

	/** Array of TLB entries to flush when unlocking page map.
	 * @note		If the count becomes greater than the array
	 *			size, then the entire TLB will be flushed. */
	ptr_t pages_to_invalidate[INVALIDATE_ARRAY_SIZE];
	size_t invalidate_count;
} page_map_t;

#endif /* __ASM__/LOADER */
#endif /* __ARCH_PAGE_H */
