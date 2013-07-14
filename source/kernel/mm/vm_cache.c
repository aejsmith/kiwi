/*
 * Copyright (C) 2010-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Page-based data cache.
 *
 * @todo		Put pages in the pageable queue.
 * @todo		Implement nonblocking I/O?
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <io/request.h>

#include <mm/phys.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <proc/thread.h>

#include <assert.h>
#include <kdb.h>
#include <status.h>

#if CONFIG_CACHE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab cache for allocating VM cache structures. */
static slab_cache_t *vm_cache_cache;

/** Constructor for VM cache structures.
 * @param obj		Object to construct.
 * @param data		Unused. */
static void vm_cache_ctor(void *obj, void *data) {
	vm_cache_t *cache = obj;

	mutex_init(&cache->lock, "vm_cache_lock", 0);
	avl_tree_init(&cache->pages);
}

/** Allocate a new VM cache.
 * @param size		Size of the cache.
 * @param ops		Pointer to operations structure (optional).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to cache structure. */
vm_cache_t *vm_cache_create(offset_t size, vm_cache_ops_t *ops, void *data) {
	vm_cache_t *cache;

	cache = slab_cache_alloc(vm_cache_cache, MM_KERNEL);
	cache->size = size;
	cache->ops = ops;
	cache->data = data;
	cache->deleted = false;
	return cache;
}

/** Get a page from a cache.
 * @note		Should not be passed both mappingp and pagep.
 * @param cache		Cache to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in if
 *			it is not in the cache, a page will only be allocated.
 *			This is used if the page is about to be overwritten.
 * @param pagep		Where to store pointer to page structure.
 * @param mappingp	Where to store address of virtual mapping. If this is
 *			set the calling thread will be wired to its CPU when
 *			the function returns.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared. Only used if mappingp is set.
 * @return		Status code describing result of the operation. */
static status_t vm_cache_get_page_internal(vm_cache_t *cache, offset_t offset,
	bool overwrite, page_t **pagep, void **mappingp, bool *sharedp)
{
	void *mapping = NULL;
	bool shared = false;
	page_t *page;
	status_t ret;

	assert((pagep && !mappingp) || (mappingp && !pagep));
	assert(!(offset % PAGE_SIZE));

	mutex_lock(&cache->lock);

	assert(!cache->deleted);

	/* Check whether it is within the size of the cache. */
	if(offset >= cache->size) {
		mutex_unlock(&cache->lock);
		return STATUS_INVALID_ADDR;
	}

	/* Check if we have it cached. */
	page = avl_tree_lookup(&cache->pages, offset, page_t, avl_link);
	if(page) {
		if(refcount_inc(&page->count) == 1)
			page_set_state(page, PAGE_STATE_ALLOCATED);

		mutex_unlock(&cache->lock);

		/* Map it in if required. Wire the thread to the current CPU
		 * and specify that the mapping is not being shared - the
		 * mapping will only be accessed by this thread, so we can
		 * save having to do a remote TLB invalidation. */
		if(mappingp) {
			assert(sharedp);

			thread_wire(curr_thread);
			*mappingp = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);
			*sharedp = false;
		} else {
			*pagep = page;
		}

		dprintf("cache: retreived cached page 0x%" PRIxPHYS " from offset "
			"0x%" PRIx64 " in %p\n", page->addr, offset, cache);
		return STATUS_SUCCESS;
	}

	/* Allocate a new page. */
	page = page_alloc(MM_KERNEL);

	/* Only bother filling the page with data if it's not going to be
	 * immediately overwritten. */
	if(!overwrite) {
		/* If a read operation is provided, read in data, else zero
		 * the page. */
		if(cache->ops && cache->ops->read_page) {
			/* When reading in page data we cannot guarantee that
			 * the mapping won't be shared, because it's possible
			 * that device driver will do work in another thread,
			 * which may be on another CPU. */
			mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);
			shared = true;

			ret = cache->ops->read_page(cache, mapping, offset);
			if(ret != STATUS_SUCCESS) {
				phys_unmap(mapping, PAGE_SIZE, true);
				page_free(page);
				mutex_unlock(&cache->lock);
				return ret;
			}
		} else {
			thread_wire(curr_thread);
			mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);
			memset(mapping, 0, PAGE_SIZE);
		}
	}

	/* Cache the page and unlock. */
	refcount_inc(&page->count);
	page->cache = cache;
	page->offset = offset;
	avl_tree_insert(&cache->pages, offset, &page->avl_link);
	mutex_unlock(&cache->lock);

	dprintf("cache: cached new page 0x%" PRIxPHYS " at offset 0x%" PRIx64
		" in %p\n", page->addr, offset, cache);

	if(mappingp) {
		assert(sharedp);

		/* Reuse any mapping that may have already been created. */
		if(!mapping) {
			thread_wire(curr_thread);
			mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);
		}

		*mappingp = mapping;
		*sharedp = shared;
	} else {
		/* Page mapping is not required, get rid of it. */
		if(mapping) {
			phys_unmap(mapping, PAGE_SIZE, shared);
			if(!shared)
				thread_unwire(curr_thread);
		}

		*pagep = page;
	}

	return STATUS_SUCCESS;
}

/** Release a page from a cache.
 * @param cache		Cache that the page belongs to.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied. */
static void vm_cache_release_page_internal(vm_cache_t *cache, offset_t offset, bool dirty) {
	page_t *page;

	mutex_lock(&cache->lock);

	assert(!cache->deleted);

	page = avl_tree_lookup(&cache->pages, offset, page_t, avl_link);
	if(unlikely(!page))
		fatal("Tried to release page that isn't cached");

	dprintf("cache: released page 0x%" PRIxPHYS " at offset 0x%" PRIx64 " "
		"in %p\n", page->addr, offset, cache);

	/* Mark as modified if requested. */
	if(dirty)
		page->modified = true;

	/* Decrease the reference count. */
	if(refcount_dec(&page->count) == 0) {
		/* If the page is outside of the cache's size (i.e. cache has
		 * been resized with pages in use, discard it). Otherwise,
		 * move the page to the appropriate queue. */
		if(offset >= cache->size) {
			avl_tree_remove(&cache->pages, &page->avl_link);
			page_free(page);
		} else if(page->modified && cache->ops && cache->ops->write_page) {
			page_set_state(page, PAGE_STATE_MODIFIED);
		} else {
			page->modified = false;
			page_set_state(page, PAGE_STATE_CACHED);
		}
	}

	mutex_unlock(&cache->lock);
}

/** Get and map a page from a cache.
 * @param cache		Cache to get page from.
 * @param offset	Offset of page to get.
 * @param overwrite	If true, then the page's data will not be read in if
 *			it is not in the cache, a page will only be allocated.
 *			This is used if the page is about to be overwritten.
 * @param addrp		Where to store address of mapping.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared.
 * @return		Status code describing result of the operation. */
static status_t vm_cache_map_page(vm_cache_t *cache, offset_t offset, bool overwrite,
	void **addrp, bool *sharedp)
{
	assert(addrp && sharedp);

	return vm_cache_get_page_internal(cache, offset, overwrite, NULL, addrp,
		sharedp);
}

/** Unmap and release a page from a cache.
 * @param cache		Cache to release page in.
 * @param addr		Address of mapping.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied.
 * @param shared	Shared value returned from vm_cache_map_page(). */
static void vm_cache_unmap_page(vm_cache_t *cache, void *mapping, offset_t offset,
	bool dirty, bool shared)
{
	phys_unmap(mapping, PAGE_SIZE, shared);
	if(!shared)
		thread_unwire(curr_thread);

	vm_cache_release_page_internal(cache, offset, dirty);
}

/** Perform I/O on a cache.
 * @param cache		Cache to read from.
 * @param request	I/O request to perform.
 * @return		Status code describing result of the operation. */
status_t vm_cache_io(vm_cache_t *cache, io_request_t *request) {
	size_t total, count;
	offset_t start, end;
	void *mapping;
	bool shared;
	status_t ret;

	mutex_lock(&cache->lock);

	/* Ensure that we do not go pass the end of the cache. */
	if(request->offset >= cache->size || !request->total) {
		mutex_unlock(&cache->lock);
		return STATUS_SUCCESS;
	}

	total = ((offset_t)(request->offset + request->total) > cache->size)
		? (size_t)(cache->size - request->offset)
		: request->total;

	mutex_unlock(&cache->lock);

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(request->offset, PAGE_SIZE);
	end = ROUND_DOWN((request->offset + (request->total - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(request->offset % PAGE_SIZE) {
		ret = vm_cache_map_page(cache, start, false, &mapping, &shared);
		if(ret != STATUS_SUCCESS)
			return ret;

		count = (start != end)
			? (size_t)(PAGE_SIZE - (request->offset % PAGE_SIZE))
			: request->total;

		io_request_copy(request, mapping + (request->offset % PAGE_SIZE), count);
		vm_cache_unmap_page(cache, mapping, start, false, shared);

		total -= count;
		start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	while(total >= PAGE_SIZE) {
		/* For writes, we pass the overwrite parameter as true to
		 * vm_cache_map_page() here, so that if the page is not in the
		 * cache, its data will not be read in - we're about to
		 * overwrite it, so it would not be necessary. */
		ret = vm_cache_map_page(cache, start, request->op == IO_OP_WRITE,
			&mapping, &shared);
		if(ret != STATUS_SUCCESS)
			return ret;

		io_request_copy(request, mapping, PAGE_SIZE);
		vm_cache_unmap_page(cache, mapping, start, false, shared);

		total -= PAGE_SIZE;
		start += PAGE_SIZE;
	}

	/* Handle anything that's left. */
	if(total) {
		ret = vm_cache_map_page(cache, start, false, &mapping, &shared);
		if(ret != STATUS_SUCCESS)
			return ret;

		io_request_copy(request, mapping, total);
		vm_cache_unmap_page(cache, mapping, start, false, shared);
	}

	return STATUS_SUCCESS;
}

/**
 * Get a page from a cache.
 *
 * Gets a page from a cache. This is a helper function to allow the cache to
 * be memory-mapped.
 *
 * @param cache		Cache to get page from.
 * @param offset	Offset into cache to get page from.
 * @param physp		Where to store physical address of page.
 *
 * @return		Status code describing result of the operation.
 */
status_t vm_cache_get_page(vm_cache_t *cache, offset_t offset, phys_ptr_t *physp) {
	status_t ret;
	page_t *page;

	ret = vm_cache_get_page_internal(cache, offset, false, &page, NULL, NULL);
	if(ret == STATUS_SUCCESS)
		*physp = page->addr;

	return ret;
}

/** Release a page in a cache.
 * @param cache		Cache to release from.
 * @param offset	Offset into cache page was from.
 * @param phys		Physical address of page. */
void vm_cache_release_page(vm_cache_t *cache, offset_t offset, phys_ptr_t phys) {
	/* The VM system will have flagged the page as modified if necessary. */
	vm_cache_release_page_internal(cache, offset, false);
}

/** Resize a cache.
 * @param cache		Cache to resize.
 * @param size		New size of the cache. */
void vm_cache_resize(vm_cache_t *cache, offset_t size) {
	page_t *page;

	mutex_lock(&cache->lock);

	/* Shrink the cache if the new size is smaller. If any pages are in use
	 * they will get freed once they are released. */
	if(size < cache->size) {
		AVL_TREE_FOREACH_SAFE(&cache->pages, iter) {
			page = avl_tree_entry(iter, page_t, avl_link);

			if(page->offset >= size && refcount_get(&page->count) == 0) {
				avl_tree_remove(&cache->pages, &page->avl_link);
				page_free(page);
			}
		}
	}

	cache->size = size;

	mutex_unlock(&cache->lock);
}

/** Flush changes to a cache page.
 * @param cache		Cache page belongs to.
 * @param page		Page to flush.
 * @return		Status code describing result of the operation. */
static status_t vm_cache_flush_page_internal(vm_cache_t *cache, page_t *page) {
	void *mapping;
	status_t ret;

	/* If the page is outside of the cache, it may be there because the
	 * cache was shrunk but with the page in use. Ignore this. Also ignore
	 * pages that aren't modified. */
	if(page->offset >= cache->size || !page->modified)
		return STATUS_SUCCESS;

	/* Should only end up here if the page is writable - when releasing
	 * pages the modified flag is cleared if there is no write operation. */
	assert(cache->ops && cache->ops->write_page);

	mapping = phys_map(page->addr, PAGE_SIZE, MM_KERNEL);

	ret = cache->ops->write_page(cache, mapping, page->offset);
	if(ret == STATUS_SUCCESS) {
		/* Clear modified flag only if the page reference count is
		 * zero. This is because the page may be mapped into an address
		 * space as read-write. */
		if(refcount_get(&page->count) == 0) {
			page->modified = false;
			page_set_state(page, PAGE_STATE_CACHED); 
		}
	}

	phys_unmap(mapping, PAGE_SIZE, true);
	return ret;
}

/** Flush modifications to a cache.
 * @param cache		Cache to flush.
 * @return		Status code describing result of the operation. If a
 *			failure occurs, the function carries on attempting to
 *			flush, but still returns an error. If multiple errors
 *			occur, it is the most recent that is returned. */
status_t vm_cache_flush(vm_cache_t *cache) {
	status_t ret = STATUS_SUCCESS, err;
	page_t *page;

	mutex_lock(&cache->lock);

	/* Flush all pages. */
	AVL_TREE_FOREACH(&cache->pages, iter) {
		page = avl_tree_entry(iter, page_t, avl_link);

		err = vm_cache_flush_page_internal(cache, page);
		if(err != STATUS_SUCCESS)
			ret = err;
	}

	mutex_unlock(&cache->lock);
	return ret;
}

/** Destroy a cache.
 * @param cache		Cache to destroy. Should NOT be in use.
 * @param discard	Whether to discard modifications. The function will
 *			always succeed if true.
 * @return		Status code describing result of the operation. */
status_t vm_cache_destroy(vm_cache_t *cache, bool discard) {
	status_t ret;
	page_t *page;

	mutex_lock(&cache->lock);
	cache->deleted = true;

	/* Free all pages. */
	AVL_TREE_FOREACH_SAFE(&cache->pages, iter) {
		page = avl_tree_entry(iter, page_t, avl_link);

		if(refcount_get(&page->count) != 0) {
			fatal("Cache page still in use while destroying");
		} else if(!discard) {
			ret = vm_cache_flush_page_internal(cache, page);
			if(ret != STATUS_SUCCESS) {
				cache->deleted = false;
				mutex_unlock(&cache->lock);
				return ret;
			}
		}

		avl_tree_remove(&cache->pages, &page->avl_link);
		page_free(page);
	}

	/* Unlock and relock the cache to allow any attempts to flush or evict
	 * a page see the deleted flag. */
	mutex_unlock(&cache->lock);
	mutex_lock(&cache->lock);
	mutex_unlock(&cache->lock);

	slab_cache_free(vm_cache_cache, cache);
	return STATUS_SUCCESS;
}

/**
 * Flush changes to a page from a cache.
 *
 * Flushes changes to a modified page belonging to a cache. This is a helper
 * function for use by the page daemon, and should not be used by anything
 * else.
 *
 * @param page		Page to flush.
 *
 * @return		Whether the page was removed from the queue.
 */
bool vm_cache_flush_page(page_t *page) {
	vm_cache_t *cache;
	status_t ret;

	/* Must be careful - another thread could be destroying the cache. */
	if(!(cache = page->cache))
		return true;

	mutex_lock(&cache->lock);

	if(cache->deleted) {
		mutex_unlock(&cache->lock);
		return true;
	}

	ret = vm_cache_flush_page_internal(cache, page);
	mutex_unlock(&cache->lock);
	return (ret == STATUS_SUCCESS);
}

/**
 * Evict a page in a cache from memory.
 *
 * Attempts to evict a page belonging to a cache from memory. This is a helper
 * function for use by the page daemon, and should not be used by anything
 * else.
 *
 * @param page		Page to evict.
 */
void vm_cache_evict_page(page_t *page) {
	vm_cache_t *cache;

	/* Must be careful - another thread could be destroying the cache. */
	if(!(cache = page->cache))
		return;

	mutex_lock(&cache->lock);

	if(cache->deleted) {
		mutex_unlock(&cache->lock);
		return;
	}

	avl_tree_remove(&cache->pages, &page->avl_link);
	page_free(page);
	mutex_unlock(&cache->lock);
}

/** Print information about a cache.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_cache(int argc, char **argv, kdb_filter_t *filter) {
	vm_cache_t *cache;
	uint64_t addr;
	page_t *page;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <address>\n\n", argv[0]);

		kdb_printf("Prints information about a VM cache.\n");
		return KDB_SUCCESS;
	} else if(argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	/* Get the address. */
	if(kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;
	cache = (vm_cache_t *)((ptr_t)addr);

	/* Print out basic information. */
	kdb_printf("Cache %p\n", cache);
	kdb_printf("=================================================\n");

	kdb_printf("Locked:  %d (%" PRId32 ")\n", atomic_get(&cache->lock.value),
		(cache->lock.holder) ? cache->lock.holder->id : -1);
	kdb_printf("Size:    %" PRIu64 "\n", cache->size);
	kdb_printf("Ops:     %p\n", cache->ops);
	kdb_printf("Data:    %p\n", cache->data);
	kdb_printf("Deleted: %d\n\n", cache->deleted);

	/* Show all cached pages. */
	kdb_printf("Cached pages:\n");
	AVL_TREE_FOREACH(&cache->pages, iter) {
		page = avl_tree_entry(iter, page_t, avl_link);

		kdb_printf("  Page 0x%016" PRIxPHYS " - Offset: %-10" PRIu64 " Modified: %-1d Count: %d\n",
			page->addr, page->offset, page->modified, refcount_get(&page->count));
	}

	return KDB_SUCCESS;
}

/** Create the VM cache structure slab cache. */
__init_text void vm_cache_init(void) {
	vm_cache_cache = slab_cache_create("vm_cache_cache", sizeof(vm_cache_t),
		0, vm_cache_ctor, NULL, NULL, 0, MM_BOOT);

	kdb_register_command("cache", "Print information about a page cache.",
		kdb_cmd_cache);
}
