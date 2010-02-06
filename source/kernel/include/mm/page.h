/* Kiwi physical memory management
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
 * @brief		Physical memory management.
 */

#ifndef __MM_PAGE_H
#define __MM_PAGE_H

#include <arch/page.h>

#include <sync/flags.h>

#include <mm/flags.h>

#if 0
# pragma mark Page map functions.
#endif

/** Page mapping protection flags. */
#define PAGE_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define PAGE_MAP_WRITE		(1<<1)	/**< Mapping should be writable. */
#define PAGE_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */

extern page_map_t kernel_page_map;

extern int page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, int prot, int mmflag);
extern int page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern int page_map_protect(page_map_t *map, ptr_t start, ptr_t end, int prot);
extern void page_map_switch(page_map_t *map);
extern int page_map_init(page_map_t *map);
extern void page_map_destroy(page_map_t *map);

#if 0
# pragma mark Physical memory access functions.
#endif

extern void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag);
extern void page_phys_unmap(void *addr, size_t size, bool shared);

#if 0
# pragma mark Page allocation functions.
#endif

/** Flags to modify page allocation behaviour. */
#define PM_ZERO			(1<<10)	/**< Clear the page contents before returning. */

extern phys_ptr_t page_xalloc(size_t count, phys_ptr_t align, phys_ptr_t phase,
                              phys_ptr_t nocross, phys_ptr_t minaddr,
                              phys_ptr_t maxaddr, int pmflag);
extern void page_xfree(phys_ptr_t base, size_t count);

extern phys_ptr_t page_alloc(size_t count, int pmflag);
extern void page_free(phys_ptr_t base, size_t count);

extern int page_zero(phys_ptr_t addr, int mmflag);
extern int page_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag);

extern void page_range_add(phys_ptr_t start, phys_ptr_t end);
extern void page_range_mark_reclaimable(phys_ptr_t start, phys_ptr_t end);
extern void page_range_mark_reserved(phys_ptr_t start, phys_ptr_t end);

extern void page_platform_init(void);
extern void page_arch_init(void);
extern void page_init(void);
extern void page_init_reclaim(void);

#endif /* __MM_PAGE_H */
