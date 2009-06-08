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

#include <sync/mutex.h>

#include <types/avltree.h>
#include <types/list.h>
#include <types/refcount.h>

struct cache;

/** Page cache operations structure. */
typedef struct cache_ops {
	/** Get a missing page from a cache.
	 * @param cache		Cache to get page from.
	 * @param offset	Offset of page in data source.
	 * @param addrp		Where to store address of page obtained.
	 * @return		0 on success, negative error code on failure. */
	int (*get_page)(struct cache *cache, offset_t offset, phys_ptr_t *addrp);

	/** Flush changes to a page to the source.
	 * @param cache		Cache that the page is in.
	 * @param page		Address of page to flush.
	 * @param offset	Offset of page in data source.
	 * @return		0 on success, negative error code on failure. */
	int (*flush_page)(struct cache *cache, phys_ptr_t page, offset_t offset);

	/** Free a page from a cache (page will have been flushed).
	 * @param cache		Cache that the page is in.
	 * @param page		Address of page to free.
	 * @param offset	Offset of page in data source. */
	void (*free_page)(struct cache *cache, phys_ptr_t page, offset_t offset);

	/** Clean up any data associated with a cache (after pages are freed).
	 * @param cache		Cache being destroyed. */
	void (*destroy)(struct cache *cache);
} cache_ops_t;

/** Structure representing a page in a cache. */
typedef struct cache_page {
	phys_ptr_t address;		/**< Physical address of the page. */
	offset_t offset;		/**< Offset of the page in the cache. */
	refcount_t count;		/**< Reference count. */
	bool dirty;			/**< Whether the page has been dirtied. */
} cache_page_t;

/** Page cache structure. */
typedef struct cache {
	list_t header;			/**< Link to cache list. */

	mutex_t lock;			/**< Lock to protect the cache. */

	avltree_t pages;		/**< Tree of pages stored in the cache. */
	cache_ops_t *ops;		/**< Cache operations. */
	void *data;			/**< Data used by the cache backend. */
} cache_t;

extern int cache_get(cache_t *cache, offset_t offset, phys_ptr_t *addrp);
extern void cache_release(cache_t *cache, offset_t offset, bool dirty);

extern cache_t *cache_create(cache_ops_t *ops, void *data);
extern int cache_destroy(cache_t *cache);

extern void cache_init(void);

#endif /* __MM_CACHE_H */
