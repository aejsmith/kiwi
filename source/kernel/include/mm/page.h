/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Physical memory management.
 */

#ifndef __MM_PAGE_H
#define __MM_PAGE_H

#include <arch/page.h>

#include <lib/avl_tree.h>
#include <lib/list.h>
#include <lib/refcount.h>

#include <mm/flags.h>

#include <sync/spinlock.h>

struct page_queue;
struct vm_amap;
struct vm_cache;

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
	 * @note		Use of count and avl_link is up to the owner.
	 * @note		Should not have both cache and amap set. */
	refcount_t count;		/**< Reference count of the page (use is up to page user). */
	struct vm_cache *cache;		/**< Cache that the page belongs to. */
	struct vm_amap *amap;		/**< Anonymous map the page belongs to. */
	offset_t offset;		/**< Offset into the owner of the page. */
	avl_tree_node_t avl_link;	/**< Link to AVL tree for use by owner. */
} vm_page_t;

/** Enumeration of memory range types. */
typedef enum memory_type {
	MEMORY_TYPE_UC,			/**< Uncacheable. */
	MEMORY_TYPE_WC,			/**< Write Combining. */
	MEMORY_TYPE_WT,			/**< Write-through. */
	MEMORY_TYPE_WB,			/**< Write-back. */
} memory_type_t;

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
extern status_t page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, bool write,
                                bool exec, int mmflag);
extern void page_map_protect(page_map_t *map, ptr_t virt, bool write, bool exec);
extern bool page_map_remove(page_map_t *map, ptr_t virt, bool shared, phys_ptr_t *physp);
extern bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp);
extern void page_map_switch(page_map_t *map);
extern page_map_t *page_map_create(int mmflag);
extern void page_map_destroy(page_map_t *map);

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
extern bool page_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag);

extern void phys_memory_type(phys_ptr_t addr, memory_type_t *typep);
extern void phys_set_memory_type(phys_ptr_t start, size_t size, memory_type_t type);

extern void *phys_map(phys_ptr_t addr, size_t size, int mmflag);
extern void phys_unmap(void *addr, size_t size, bool shared);

extern void page_stats_get(page_stats_t *stats);

extern int kdbg_cmd_page(int argc, char **argv);

extern void page_arch_init(void);
extern void page_init(void);
extern void vm_page_init(void);
extern void page_late_init(void);

#endif /* __MM_PAGE_H */
