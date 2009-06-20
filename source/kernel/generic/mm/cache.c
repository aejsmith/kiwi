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
 *
 * @todo		Implement cache_reclaim() to reclaim unused cached
 *			pages.
 * @todo		Use a readers-writer lock - exclusive access is not
 *			needed when performing a lookup on a page already in
 *			the cache, only when inserting a new page.
 */

#include <console/kprintf.h>

#include <mm/cache.h>
#include <mm/slab.h>

#include <assert.h>
#include <fatal.h>

#if CONFIG_CACHE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** List of all page caches. */
static LIST_DECLARE(cache_list);
static MUTEX_DECLARE(cache_list_lock, 0);

/** Slab caches for cache/cache page structures. */
static slab_cache_t *cache_cache;
static slab_cache_t *cache_page_cache;

/** Constructor for cache structures.
 * @param obj		Object to construct.
 * @param data		Slab cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int cache_ctor(void *obj, void *data, int kmflag) {
	cache_t *cache = obj;

	list_init(&cache->header);
	mutex_init(&cache->lock, "cache_lock", 0);
	avltree_init(&cache->pages);
	return 0;
}

/** Constructor for cache page structures.
 * @param obj		Object to construct.
 * @param data		Slab cache data (unused).
 * @param kmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int cache_page_ctor(void *obj, void *data, int kmflag) {
	cache_page_t *page = obj;

	refcount_set(&page->count, 0);
	return 0;
}

/** Get a a page from a cache.
 *
 * Gets a page from a page cache. If the page is not in the cache, then it
 * will be pulled in from the source.
 *
 * @param cache		Cache to get page from.
 * @param offset	Offset in the cache of the page.
 * @param addrp		Where to store address of page obtained.
 *
 * @return		0 on success, negative error code on failure.
 */
int cache_get(cache_t *cache, offset_t offset, phys_ptr_t *addrp) {
	cache_page_t *page;
	phys_ptr_t addr;
	int ret;

	assert(addrp);

	mutex_lock(&cache->lock, 0);

	/* Attempt to look the page up in the cache. */
	page = avltree_lookup(&cache->pages, (key_t)offset);
	if(likely(page != NULL)) {
		refcount_inc(&page->count);
		*addrp = page->address;

		dprintf("cache: retreived cached page 0x%" PRIpp " from 0x%p:%" PRIo "\n",
		        page->address, cache, offset);
		mutex_unlock(&cache->lock);
		return 0;
	}

	/* Page is not in the cache - try to get it in from the source. */
	ret = cache->ops->get_page(cache, offset, &addr);
	if(ret != 0) {
		mutex_unlock(&cache->lock);
		return ret;
	}

	/* Allocate a page structure to track the page. */
	page = slab_cache_alloc(cache_page_cache, MM_SLEEP);
	page->address = addr;
	page->offset = offset;
	refcount_inc(&page->count);

	/* Insert it into the tree and finish. */
	avltree_insert(&cache->pages, (key_t)offset, page, NULL);
	*addrp = page->address;

	dprintf("cache: cached new page 0x%" PRIpp " in 0x%p:%" PRIo "\n",
	        page->address, cache, offset);
	mutex_unlock(&cache->lock);
	return 0;
}

/** Release a page in a cache.
 *
 * Decreases the reference count of a page within a page cache. It is an error
 * to call this function if the page is not in the cache, or its reference
 * count is already 0.
 *
 * @param cache		Cache to release page in.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied.
 */
void cache_release(cache_t *cache, offset_t offset, bool dirty) {
	cache_page_t *page;

	mutex_lock(&cache->lock, 0);

	page = avltree_lookup(&cache->pages, (key_t)offset);
	if(page == NULL) {
		fatal("Tried to release page outside of cache");
	}

	/* Dirty the page if required. */
	if(dirty) {
		page->dirty = true;
	}

	refcount_dec(&page->count);

	dprintf("cache: released page 0x%" PRIpp " at 0x%p:%" PRIo "\n",
	        page->address, cache, offset);
	mutex_unlock(&cache->lock);
}

/** Create a new page cache.
 *
 * Creates a new page cache structure. The operations structure should specify
 * at least the get_page and free_page operations.
 *
 * @param ops		Page cache operations structure.
 * @param data		Data associated with the cache.
 *
 * @return		Pointer to cache structure allocated.
 */
cache_t *cache_create(cache_ops_t *ops, void *data) {
	cache_t *cache = slab_cache_alloc(cache_cache, MM_SLEEP);

	assert(ops->get_page);
	assert(ops->free_page);

	cache->ops = ops;
	cache->data = data;

	mutex_lock(&cache_list_lock, 0);
	list_append(&cache_list, &cache->header);
	mutex_unlock(&cache_list_lock);

	return cache;
}

/** Destroy a page cache.
 *
 * Flushes and frees any pages still existing in a page cache and destroys
 * it. It is possible for this function to return an error during cache
 * flushing, as the flush_page operation can return an error. It is an error
 * to destroy the cache if any of its pages are still in use.
 *
 * @param cache		Cache to destroy.
 *
 * @return		0 on success, negative error code on failure.
 */
int cache_destroy(cache_t *cache) {
	avltree_node_t *iter;
	cache_page_t *page;
	int ret;

	mutex_lock(&cache->lock, 0);

	/* Flush and free all pages in the cache. The AVLTREE_FOREACH iterator
	 * is not safe to use when removing entries from the tree. */
	while((iter = avltree_node_first(&cache->pages)) != NULL) {
		page = avltree_entry(iter, cache_page_t);

		if(refcount_get(&page->count) != 0) {
			fatal("Attempted to destroy cache still in use");
		}

		/* Flush the page if the cache wants us to and the page has
		 * been dirtied. */
		if(cache->ops->flush_page != NULL && page->dirty) {
			ret = cache->ops->flush_page(cache, page->address, page->offset);
			if(ret != 0 && ret != 1) {
				dprintf("cache: failed to flush entry %" PRIo " (0x%" PRIpp ") in 0x%p: %d\n",
				        page->offset, page->address, cache, ret);
				mutex_unlock(&cache->lock);
				return ret;
			}
		}

		/* Free the page. */
		cache->ops->free_page(cache, page->address, page->offset);
		avltree_remove(&cache->pages, (key_t)page->offset);
		slab_cache_free(cache_page_cache, page);
	}

	/* Call any destructor provided on the cache. */
	if(cache->ops->destroy != NULL) {
		cache->ops->destroy(cache);
	}

	mutex_lock(&cache_list_lock, 0);
	list_remove(&cache->header);
	mutex_unlock(&cache_list_lock);

	mutex_unlock(&cache->lock);
	slab_cache_free(cache_cache, cache);
	return 0;
}

/** Initialize the cache subsystem. */
void cache_init(void) {
	cache_cache = slab_cache_create("cache_cache", sizeof(cache_t), 0,
	                                cache_ctor, NULL, NULL, NULL, NULL,
	                                0, MM_FATAL);
	cache_page_cache = slab_cache_create("cache_page_cache", sizeof(cache_page_t),
	                                     0, cache_page_ctor, NULL, NULL, NULL,
	                                     NULL, 0, MM_FATAL);
}
