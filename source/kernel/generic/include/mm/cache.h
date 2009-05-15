/* Kiwi page cache manager
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
 * @brief		Page cache manager.
 */

#ifndef __MM_CACHE_H
#define __MM_CACHE_H

#include <lib/refcount.h>

#include <sync/mutex.h>

#include <types/avltree.h>

struct page;
struct page_cache;

/** Page cache operations structure. */
typedef struct page_cache_ops {
	/** Get a missing page from a cache.
	 * @param cache		Cache to get page from.
	 * @param pagep		Where to store pointer to page obtained.
	 * @return		1 if page obtained and should be cached,
	 *			0 if page obtained but should not be cached,
	 *			negative error code on failure. */
	int (*get_page)(struct page_cache *cache, offset_t offset, struct page **pagep);

	/** Flush changes to a page to the source.
	 * @param cache		Cache that the page is in.
	 * @param page		Page to flush.
	 * @return		0 on success, negative error code on failure. */
	int (*flush_page)(struct page_cache *cache, struct page *page, offset_t offset);

	/** Free a page from a cache (page will have been flushed).
	 * @param cache		Cache that the page is in.
	 * @param page		Page to free. */
	void (*free_page)(struct page_cache *cache, struct page *page, offset_t offset);

	/** Clean up any data associated with a cache (after pages are freed).
	 * @param cache		Cache being destroyed. */
	void (*destroy_cache)(struct page_cache *cache);
} page_cache_ops_t;

/** Page cache structure. */
typedef struct page_cache {
	mutex_t lock;			/**< Lock to protect the cache. */

	avltree_t pages;		/**< Tree of pages stored in the cache. */
	page_cache_ops_t *ops;		/**< Cache operations. */
	void *data;			/**< Data used by the cache backend. */

	refcount_t count;		/**< Reference count. */
} page_cache_t;

extern page_cache_t *page_cache_create(page_cache_ops_t *ops, void *data);
extern bool page_cache_destroy(page_cache_t *cache);

#endif /* __MM_CACHE_H */
