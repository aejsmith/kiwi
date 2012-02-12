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

#include <mm/mm.h>

#include <platform/page.h>

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
typedef struct page {
	list_t header;			/**< Link to page queue. */

	/** Basic page information. */
	phys_ptr_t addr;		/**< Physical address of page. */
	unsigned phys_range;		/**< Memory range that the page belongs to. */
	unsigned state;			/**< State of the page. */
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
} page_t;

/** Possible states of a page. */
#define PAGE_STATE_ALLOCATED	0	/**< Allocated. */
#define PAGE_STATE_MODIFIED	1	/**< Modified. */
#define PAGE_STATE_CACHED	2	/**< Cached. */
#define PAGE_STATE_FREE		3	/**< Free. */

extern void page_set_state(page_t *page, unsigned state);
extern page_t *page_lookup(phys_ptr_t addr);
extern page_t *page_alloc(int mmflag);
extern void page_free(page_t *page);
extern page_t *page_copy(page_t *page, int mmflag);

extern void page_stats_get(page_stats_t *stats);

extern void page_add_physical_range(phys_ptr_t start, phys_ptr_t end, unsigned freelist);

extern void platform_page_init(void);

extern void page_init(void);
extern void page_daemon_init(void);
extern void page_late_init(void);

#endif /* __MM_PAGE_H */
