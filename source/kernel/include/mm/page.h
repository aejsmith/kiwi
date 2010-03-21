/*
 * Copyright (C) 2009-2010 Alex Smith
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
#include <lib/list.h>
#include <lib/refcount.h>

#include <mm/flags.h>
#include <sync/spinlock.h>

struct kernel_args;
struct object;
struct page_queue;
struct vm_amap;

/** Structure containing physical memory usage statistics. */
typedef struct page_stats {
	uint64_t total;			/**< Total available memory. */
	uint64_t allocated;		/**< Amount of memory in-use. */
	uint64_t modified;		/**< Amount of memory containing modified data. */
	uint64_t cached;		/**< Amount of memory being used by caches. */
	uint64_t free;			/**< Amount of free memory. */
} page_stats_t;

/** Structure describing a page in memory. */
typedef struct vm_page {
	list_t header;			/**< Link to page queue. */
	struct page_queue *queue;	/**< Queue that the page is in. */

	/** Basic page information. */
	phys_ptr_t addr;		/**< Physical address of the page. */
	bool modified : 1;		/**< Whether the page has been modified. */
	uint8_t unused: 7;

	/** Information about how the page is being used.
	 * @note		Use of count is up to the page owner.
	 * @note		Should not have both object and amap set. */
	refcount_t count;		/**< Reference count of the page (use is up to page user). */
	struct object *object;		/**< Object that the page belongs to. */
	struct vm_amap *amap;		/**< Anonymous map the page belongs to. */
	offset_t offset;		/**< Offset into the object of the page. */
} vm_page_t;

/** Page queue numbers. */
#define PAGE_QUEUE_MODIFIED	0	/**< Pages that need to be written. */
#define PAGE_QUEUE_CACHED	1	/**< Pages that are held in caches. */
#define PAGE_QUEUE_PAGEABLE	2	/**< Pages that are mapped but can be paged out. */
#define PAGE_QUEUE_COUNT	3	/**< Number of page lists. */

/** Flags to modify page allocation behaviour. */
#define PM_ZERO			(1<<10)	/**< Clear the page contents before returning. */

extern page_map_t kernel_page_map;

extern void page_map_lock(page_map_t *map);
extern void page_map_unlock(page_map_t *map);
extern int page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, bool write,
                           bool exec, int mmflag);
extern bool page_map_remove(page_map_t *map, ptr_t virt, bool shared, phys_ptr_t *physp);
extern bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern void page_map_switch(page_map_t *map);
extern int page_map_init(page_map_t *map, int mmflag);
extern void page_map_destroy(page_map_t *map);

extern void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag);
extern void page_phys_unmap(void *addr, size_t size, bool shared);

extern vm_page_t *vm_page_alloc(size_t count, int pmflag);
extern void vm_page_free(vm_page_t *pages, size_t count);
extern vm_page_t *vm_page_copy(vm_page_t *page, int mmflag);
extern void vm_page_queue(vm_page_t *page, size_t queue);
extern void vm_page_dequeue(vm_page_t *page);
extern vm_page_t *vm_page_lookup(phys_ptr_t addr);

extern phys_ptr_t page_xalloc(size_t count, phys_ptr_t align, phys_ptr_t minaddr,
                              phys_ptr_t maxaddr, int pmflag);
extern phys_ptr_t page_alloc(size_t count, int pmflag);
extern void page_free(phys_ptr_t base, size_t count);
extern int page_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag);

extern void page_stats_get(page_stats_t *stats);

extern int kdbg_cmd_page(int argc, char **argv);

extern void page_arch_init(struct kernel_args *args);
extern void page_init(struct kernel_args *args);
extern void vm_page_init(void);
extern void page_arch_late_init(void);
extern void page_late_init(void);

#endif /* __MM_PAGE_H */
