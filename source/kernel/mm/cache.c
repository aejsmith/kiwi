/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Page-based data cache.
 *
 * @todo		Put pages in the pageable queue.
 * @todo		Make nonblocking I/O actually work properly.
 */

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/cache.h>
#include <mm/slab.h>

#include <proc/thread.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>

#if CONFIG_CACHE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab cache for allocating VM cache structures. */
static slab_cache_t *vm_cache_cache;

/** Constructor for VM cache structures.
 * @param obj		Object to construct.
 * @param data		Unused.
 * @param mmflag	Allocation flags.
 * @return		Always returns 0. */
static int vm_cache_ctor(void *obj, void *data, int mmflag) {
	vm_cache_t *cache = obj;

	mutex_init(&cache->lock, "vm_cache_lock", 0);
	avl_tree_init(&cache->pages);
	return 0;
}

/** Allocate a new VM cache.
 * @param size		Size of the cache.
 * @param ops		Pointer to operations structure (optional).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to cache structure. */
vm_cache_t *vm_cache_create(offset_t size, vm_cache_ops_t *ops, void *data) {
	vm_cache_t *cache;

	cache = slab_cache_alloc(vm_cache_cache, MM_SLEEP);
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
 * @param nonblock	Whether the operation is required to not block.
 * @param pagep		Where to store pointer to page structure.
 * @param mappingp	Where to store address of virtual mapping. If this is
 *			set the calling thread will be wired to its CPU when
 *			the function returns.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared. Only used if mappingp is set.
 * @return		0 on success, negative error code on failure. */
static int vm_cache_get_page_internal(vm_cache_t *cache, offset_t offset, bool overwrite,
                                      bool nonblock, vm_page_t **pagep, void **mappingp,
                                      bool *sharedp) {
	void *mapping = NULL;
	bool shared = false;
	vm_page_t *page;
	int ret;

	assert((pagep && !mappingp) || (mappingp && !pagep));
	assert(!(offset % PAGE_SIZE));

	mutex_lock(&cache->lock);

	assert(!cache->deleted);

	/* Check whether it is within the size of the cache. */
	if(offset >= cache->size) {
		mutex_unlock(&cache->lock);
		return -ERR_NOT_FOUND;
	}

	/* Check if we have it cached. */
	if((page = avl_tree_lookup(&cache->pages, (key_t)offset))) {
		if(refcount_inc(&page->count) == 1) {
			vm_page_dequeue(page);
		}
		mutex_unlock(&cache->lock);

		/* Map it in if required. Wire the thread to the current CPU
		 * and specify that the mapping is not being shared - the
		 * mapping will only be accessed by this thread, so we can
		 * save having to do a remote TLB invalidation. */
		if(mappingp) {
			assert(sharedp);

			thread_wire(curr_thread);
			*mappingp = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
			*sharedp = false;
		} else {
			*pagep = page;
		}

		dprintf("cache: retreived cached page 0x%" PRIpp " from offset 0x%" PRIx64 " in %p\n",
		        page->addr, offset, cache);
		return 0;
	}

	/* Allocate a new page. */
	page = vm_page_alloc(1, MM_SLEEP);

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
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
			shared = true;

			if((ret = cache->ops->read_page(cache, mapping, offset, nonblock)) != 0) {
				page_phys_unmap(mapping, PAGE_SIZE, true);
				vm_page_free(page, 1);
				mutex_unlock(&cache->lock);
				return ret;
			}
		} else {
			thread_wire(curr_thread);
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
			memset(mapping, 0, PAGE_SIZE);
		}
	}

	/* Cache the page and unlock. */
	refcount_inc(&page->count);
	page->cache = cache;
	page->offset = offset;
	avl_tree_insert(&cache->pages, (key_t)offset, page, NULL);
	mutex_unlock(&cache->lock);

	dprintf("cache: cached new page 0x%" PRIpp " at offset 0x%" PRIx64 " in %p\n",
	        page->addr, offset, cache);

	if(mappingp) {
		assert(sharedp);

		/* Reuse any mapping that may have already been created. */
		if(!mapping) {
			thread_wire(curr_thread);
			mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);
		}

		*mappingp = mapping;
		*sharedp = shared;
	} else {
		/* Page mapping is not required, get rid of it. */
		if(mapping) {
			page_phys_unmap(mapping, PAGE_SIZE, shared);
			if(!shared) {
				thread_unwire(curr_thread);
			}
		}
		*pagep = page;
	}
	return 0;
}

/** Release a page from a cache.
 * @param Cache		Cache that the page belongs to.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied. */
static void vm_cache_release_page_internal(vm_cache_t *cache, offset_t offset, bool dirty) {
	vm_page_t *page;

	mutex_lock(&cache->lock);

	assert(!cache->deleted);

	if(!(page = avl_tree_lookup(&cache->pages, (key_t)offset))) {
		fatal("Tried to release page that isn't cached");
	}

	dprintf("cache: released page 0x%" PRIpp " at offset 0x%" PRIx64 " in %p\n",
	        page->addr, offset, cache);

	/* Mark as modified if requested. */
	if(dirty) {
		page->modified = true;
	}

	/* Decrease the reference count. */
	if(refcount_dec(&page->count) == 0) {
		/* If the page is outside of the cache's size (i.e. cache has
		 * been resized with pages in use, discard it). Otherwise,
		 * move the page to the appropriate queue. */
		if(offset >= cache->size) {
			avl_tree_remove(&cache->pages, (key_t)offset);
			vm_page_free(page, 1);
		} else if(page->modified && cache->ops && cache->ops->write_page) {
			vm_page_queue(page, PAGE_QUEUE_MODIFIED);
		} else {
			page->modified = false;
			vm_page_queue(page, PAGE_QUEUE_CACHED);
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
 * @param nonblock	Whether the operation is required to not block.
 * @param addrp		Where to store address of mapping.
 * @param sharedp	Where to store value stating whether a mapping had to
 *			be shared.
 * @return		0 on success, negative error code on failure. */
static int vm_cache_map_page(vm_cache_t *cache, offset_t offset, bool overwrite,
                             bool nonblock, void **addrp, bool *sharedp) {
	assert(addrp && sharedp);
	return vm_cache_get_page_internal(cache, offset, overwrite, nonblock, NULL, addrp, sharedp);
}

/** Unmap and release a page from a cache.
 * @param cache		Cache to release page in.
 * @param addr		Address of mapping.
 * @param offset	Offset of page to release.
 * @param dirty		Whether the page has been dirtied.
 * @param shared	Shared value returned from vm_cache_map_page(). */
static void vm_cache_unmap_page(vm_cache_t *cache, void *mapping, offset_t offset,
                                bool dirty, bool shared) {
	page_phys_unmap(mapping, PAGE_SIZE, shared);
	if(!shared) {
		thread_unwire(curr_thread);
	}
	vm_cache_release_page_internal(cache, offset, dirty);
}

/** Read data from a cache.
 * @param cache		Cache to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into cache to read from.
 * @param nonblock	Whether the operation is required to not block.
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. Upon
 *			failure, part of the data may have been read, in which
 *			case the number of bytes read successfully are stored
 *			in bytesp. */
int vm_cache_read(vm_cache_t *cache, void *buf, size_t count, offset_t offset,
                  bool nonblock, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	bool shared;
	int ret = 0;

	mutex_lock(&cache->lock);

	/* Ensure that we do not go pass the end of the cache. */
	if(offset >= cache->size || count == 0) {
		mutex_unlock(&cache->lock);
		goto out;
	} else if((offset + count) >= cache->size) {
		count = (size_t)(cache->size - offset);
	}

	mutex_unlock(&cache->lock);

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		if((ret = vm_cache_map_page(cache, start, false, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(buf, mapping + (offset % PAGE_SIZE), size);
		vm_cache_unmap_page(cache, mapping, start, false, shared);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vm_cache_map_page(cache, start, false, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, PAGE_SIZE);
		vm_cache_unmap_page(cache, mapping, start, false, shared);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vm_cache_map_page(cache, start, false, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(buf, mapping, count);
		vm_cache_unmap_page(cache, mapping, start, false, shared);
		total += count;
	}
out:
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Write data to a cache.
 * @param cache		Cache to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset into cache to write to.
 * @param nonblock	Whether the operation is required to not block.
 * @param bytesp	Where to store number of bytes read.
 * @return		0 on success, negative error code on failure. Upon
 *			failure, part of the data may have been read, in which
 *			case the number of bytes read successfully are stored
 *			in bytesp. */
int vm_cache_write(vm_cache_t *cache, const void *buf, size_t count, offset_t offset,
                   bool nonblock, size_t *bytesp) {
	offset_t start, end, i, size;
	size_t total = 0;
	void *mapping;
	bool shared;
	int ret = 0;

	mutex_lock(&cache->lock);

	/* Ensure that we do not go pass the end of the cache. */
	if(offset >= cache->size || count == 0) {
		mutex_unlock(&cache->lock);
		goto out;
	} else if((offset + count) >= cache->size) {
		count = (size_t)(cache->size - offset);
	}

	mutex_unlock(&cache->lock);

	/* Now work out the start page and the end page. Subtract one from
	 * count to prevent end from going onto the next page when the offset
	 * plus the count is an exact multiple of PAGE_SIZE. */
	start = ROUND_DOWN(offset, PAGE_SIZE);
	end = ROUND_DOWN((offset + (count - 1)), PAGE_SIZE);

	/* If we're not starting on a page boundary, we need to do a partial
	 * transfer on the initial page to get us up to a page boundary. 
	 * If the transfer only goes across one page, this will handle it. */
	if(offset % PAGE_SIZE) {
		if((ret = vm_cache_map_page(cache, start, false, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		size = (start == end) ? count : (size_t)PAGE_SIZE - (size_t)(offset % PAGE_SIZE);
		memcpy(mapping + (offset % PAGE_SIZE), buf, size);
		vm_cache_unmap_page(cache, mapping, start, true, shared);
		total += size; buf += size; count -= size; start += PAGE_SIZE;
	}

	/* Handle any full pages. We pass the overwrite parameter as true to
	 * vm_cache_map_page() here, so that if the page is not in the cache,
	 * its data will not be read in - we're about to overwrite it, so it
	 * would not be necessary. */
	size = count / PAGE_SIZE;
	for(i = 0; i < size; i++, total += PAGE_SIZE, buf += PAGE_SIZE, count -= PAGE_SIZE, start += PAGE_SIZE) {
		if((ret = vm_cache_map_page(cache, start, true, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, PAGE_SIZE);
		vm_cache_unmap_page(cache, mapping, start, true, shared);
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if((ret = vm_cache_map_page(cache, start, false, nonblock, &mapping, &shared)) != 0) {
			goto out;
		}

		memcpy(mapping, buf, count);
		vm_cache_unmap_page(cache, mapping, start, true, shared);
		total += count;
	}
out:
	if(bytesp) {
		*bytesp = total;
	}
	return ret;
}

/** Get a page from a cache.
 *
 * Gets a page from a cache. This is a helper function to allow the cache to
 * be memory-mapped.
 *
 * @param cache		Cache to get page from.
 * @param offset	Offset into cache to get page from.
 * @param physp		Where to store physical address of page.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_cache_get_page(vm_cache_t *cache, offset_t offset, phys_ptr_t *physp) {
	vm_page_t *page;
	int ret;

	ret = vm_cache_get_page_internal(cache, offset, false, false, &page, NULL, NULL);
	if(ret == 0) {
		*physp = page->addr;
	}

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
	vm_page_t *page;

	mutex_lock(&cache->lock);

	/* Shrink the cache if the new size is smaller. If any pages are in use
	 * they will get freed once they are released. */
	if(size < cache->size) {
		AVL_TREE_FOREACH(&cache->pages, iter) {
			page = avl_tree_entry(iter, vm_page_t);

			if(page->offset >= size && refcount_get(&page->count) == 0) {
				avl_tree_remove(&cache->pages, (key_t)page->offset);
				vm_page_dequeue(page);
				vm_page_free(page, 1);
			}
		}
	}

	cache->size = size;

	mutex_unlock(&cache->lock);
}

/** Flush changes to a cache page.
 * @param cache		Cache page belongs to.
 * @param page		Page to flush.
 * @return		0 on success, negative error code on failure. */
static int vm_cache_flush_page_internal(vm_cache_t *cache, vm_page_t *page) {
	void *mapping;
	int ret = 0;

	/* If the page is outside of the cache, it may be there because the
	 * cache was shrunk but with the page in use. Ignore this. Also ignore
	 * pages that aren't modified. */
	if(page->offset >= cache->size || !page->modified) {
		return 0;
	}

	/* Should only end up here if the page is writable - when releasing
	 * pages the modified flag is cleared if there is no write operation. */
	assert(cache->ops && cache->ops->write_page);

	mapping = page_phys_map(page->addr, PAGE_SIZE, MM_SLEEP);

	if((ret = cache->ops->write_page(cache, mapping, page->offset, false)) == 0) {
		/* Clear modified flag only if the page reference count is
		 * zero. This is because the page may be mapped into an address
		 * space as read-write. */
		if(refcount_get(&page->count) == 0) {
			page->modified = false;
			vm_page_queue(page, PAGE_QUEUE_CACHED); 
		}
	}

	page_phys_unmap(mapping, PAGE_SIZE, true);
	return ret;
}

/** Flush modifications to a cache.
 * @param cache		Cache to flush.
 * @return		0 on success, negative error code on failure. If a
 *			failure occurs, the function carries on attempting to
 *			flush, but still returns an error. If multiple errors
 *			occur, it is the most recent that is returned. */
int vm_cache_flush(vm_cache_t *cache) {
	int err = 0, ret;
	vm_page_t *page;

	mutex_lock(&cache->lock);

	/* Flush all pages. */
	AVL_TREE_FOREACH(&cache->pages, iter) {
		page = avl_tree_entry(iter, vm_page_t);

		if((err = vm_cache_flush_page_internal(cache, page)) != 0) {
			ret = err;
		}
	}

	return err;
}

/** Destroy a cache.
 * @param cache		Cache to destroy. Should NOT be in use.
 * @param discard	Whether to discard modifications. The function will
 *			always succeed if true.
 * @return		0 on success, negative error code on failure. */
int vm_cache_destroy(vm_cache_t *cache, bool discard) {
	vm_page_t *page;
	int ret;

	mutex_lock(&cache->lock);
	cache->deleted = true;

	/* Free all pages. */
	AVL_TREE_FOREACH(&cache->pages, iter) {
		page = avl_tree_entry(iter, vm_page_t);

		if(refcount_get(&page->count) != 0) {
			fatal("Cache page still in use while destroying");
		} else if(!discard && (ret = vm_cache_flush_page_internal(cache, page)) != 0) {
			cache->deleted = false;
			mutex_unlock(&cache->lock);
			return ret;
		}

		avl_tree_remove(&cache->pages, (key_t)page->offset);
		vm_page_dequeue(page);
		vm_page_free(page, 1);
	}

	/* Unlock and relock the cache to allow any attempts to flush or evict
	 * a page see the deleted flag. */
	mutex_unlock(&cache->lock);
	mutex_lock(&cache->lock);
	mutex_unlock(&cache->lock);

	slab_cache_free(vm_cache_cache, cache);
	return 0;
}

/** Flush changes to a page from a cache.
 *
 * Flushes changes to a modified page belonging to a cache. This is a helper
 * function for use by the page daemon, and should not be used by anything
 * else.
 *
 * @param page		Page to flush.
 *
 * @return		Whether the page was removed from the queue.
 */
bool vm_cache_flush_page(vm_page_t *page) {
	vm_cache_t *cache;
	bool ret;

	/* Must be careful - another thread could be destroying the cache. */
	if(!(cache = page->cache)) {
		return true;
	}
	mutex_lock(&cache->lock);
	if(cache->deleted) {
		mutex_unlock(&cache->lock);
		return true;
	}

	ret = vm_cache_flush_page_internal(cache, page) == 0;
	mutex_unlock(&cache->lock);
	return ret;
}

/** Evict a page in a cache from memory.
 *
 * Attempts to evict a page belonging to a cache from memory. This is a helper
 * function for use by the page daemon, and should not be used by anything
 * else.
 *
 * @param page		Page to evict.
 */
//void vm_cache_evict_page(vm_page_t *page) {
//	fatal("Not implemented");
//}

/** Print information about a cache.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_cache(int argc, char **argv) {
	vm_cache_t *cache;
	vm_page_t *page;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <address>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints details about a VM cache.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	/* Get the address. */
	if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}
	cache = (vm_cache_t *)((ptr_t)val);

	/* Print out basic information. */
	kprintf(LOG_NONE, "Cache %p\n", cache);
	kprintf(LOG_NONE, "=================================================\n");

	kprintf(LOG_NONE, "Locked:  %d (%" PRId32 ")\n", atomic_get(&cache->lock.locked),
	        (cache->lock.holder) ? cache->lock.holder->id : -1);
	kprintf(LOG_NONE, "Size:    %" PRIu64 "\n", cache->size);
	kprintf(LOG_NONE, "Ops:     %p\n", cache->ops);
	kprintf(LOG_NONE, "Data:    %p\n", cache->data);
	kprintf(LOG_NONE, "Deleted: %d\n\n", cache->deleted);

	/* Show all cached pages. */
	kprintf(LOG_NONE, "Cached pages:\n");
	AVL_TREE_FOREACH(&cache->pages, iter) {
		page = avl_tree_entry(iter, vm_page_t);

		kprintf(LOG_NONE, "  Page 0x%016" PRIpp " - Offset: %-10" PRIu64 " Modified: %-1d Count: %d\n",
		        page->addr, page->offset, page->modified, refcount_get(&page->count));
	}

	return KDBG_OK;
}

/** Create the VM cache structure slab cache. */
void __init_text vm_cache_init(void) {
	vm_cache_cache = slab_cache_create("vm_cache_cache", sizeof(vm_cache_t),
	                                   0, vm_cache_ctor, NULL, NULL, NULL,
	                                   SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}
