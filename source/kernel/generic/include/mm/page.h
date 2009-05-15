/* Kiwi page mapping functions
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
 * @brief		Page mapping functions.
 *
 * This header file just contains the definition of the page map interface,
 * the actual implementation is made by the architecture.
 */

#ifndef __MM_PAGE_H
#define __MM_PAGE_H

#include <arch/page.h>

#include <mm/flags.h>

/** Page mapping protection flags. */
#define PAGE_MAP_READ		(1<<0)	/**< Mapping should be readable. */
#define PAGE_MAP_WRITE		(1<<1)	/**< Mapping should be writable. */
#define PAGE_MAP_EXEC		(1<<2)	/**< Mapping should be executable. */

extern page_map_t kernel_page_map;

/** Page map manipulation functions. */
extern bool page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, int prot, int mmflag);
extern bool page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern void page_map_switch(page_map_t *map);
extern int  page_map_init(page_map_t *map);
extern void page_map_destroy(page_map_t *map);

/** Physical memory access functions. */
extern void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag);
extern void page_phys_unmap(void *addr, size_t size);

extern void page_init(void);

#endif /* __MM_PAGE_H */
