/* Kiwi IA32 paging definitions
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
 * @brief		IA32 paging definitions.
 */

#ifndef __ARCH_PAGE_H
#define __ARCH_PAGE_H

/** Page size definitions. */
#define PAGE_WIDTH	12			/**< Width of a page in bits. */
#define PAGE_SIZE	(1<<PAGE_WIDTH)		/**< Size of a page (4KB). */
#define PAGE_MASK	0xFFFFF000		/**< Mask to clear the page offset from a virtual address. */

/** Definitions of paging structure bits. */
#define PG_PRESENT	(1<<0)			/**< Page is present. */
#define PG_WRITE	(1<<1)			/**< Page is writable. */
#define PG_USER		(1<<2)			/**< Page is accessible in CPL3. */
#define PG_PWT		(1<<3)			/**< Page has write-through caching. */
#define PG_NOCACHE	(1<<4)			/**< Page has caching disabled. */
#define PG_ACCESSED	(1<<5)			/**< Page has been accessed. */
#define PG_DIRTY	(1<<6)			/**< Page has been written to. */
#define PG_LARGE	(1<<7)			/**< Page is a large page. */
#define PG_GLOBAL	(1<<8)			/**< Page won't be cleared in TLB. */
#ifndef __ASM__
# define PG_NOEXEC	((uint64_t)1<<63)	/**< Page is not executable (requires NX support). */
#else
# define PG_NOEXEC	(1<<63)			/**< Page is not executable (requires NX support). */
#endif

#ifndef __ASM__

#include <sync/mutex.h>

#include <types.h>

/** Architecture-specific page map structure. */
typedef struct page_map {
	mutex_t lock;			/**< Lock to protect page map. */
	phys_ptr_t pdp;			/**< Physical address of PDP. */
	bool user;			/**< Whether pages mapped should be userspace accessible. */

	/** Range covered by page map. */
	ptr_t first;			/**< First allowed page. */
	ptr_t last;			/**< Last allowed page. */
} page_map_t;

/** Structure of a page table entry. */
typedef struct pte {
	unsigned present : 1;		/**< Present (P) flag. */
	unsigned writable : 1;		/**< Read/write (R/W) flag. */
	unsigned user : 1;		/**< User/supervisor (U/S) flag. */
	unsigned pwt : 1;		/**< Page-level write through (PWT) flag. */
	unsigned pcd : 1;		/**< Page-level cache disable (PCD) flag. */
	unsigned accessed : 1;		/**< Accessed (A) flag. */
	unsigned dirty : 1;		/**< Dirty (D) flag. */
	unsigned large : 1;		/**< Page size (PS) flag (page directory). */
	unsigned global : 1;		/**< Global (G) flag. */
	unsigned avail : 3;		/**< Available for use. */
	unsigned address : 24;		/**< Page base address. */
	unsigned reserved: 27;		/**< Reserved. */
	unsigned noexec : 1;		/**< No-Execute (NX) flag. */
} __packed pte_t;

/** Type that allows a page table entry to be accessed as a single value. */
typedef uint64_t pte_simple_t;

extern void page_late_init(void);

#endif /* __ASM__ */
#endif /* __ARCH_PAGE_H */
