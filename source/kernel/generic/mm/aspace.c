/* Kiwi address space management
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
 * @brief		Address space management.
 *
 * The address space manager handles the creation and manipulation of
 * per-process address spaces. An address space is made up of several parts:
 *
 * At the top level, there is a collection of mapped/reserved memory regions.
 * These represent the memory mappings within the address space. Underneath
 * a mapped region is a page source. A page source can be shared between
 * mulitple regions - sharing occurs implicitly when a region has to be split
 * for whatever reason. It can also be shared across address spaces when
 * cloning an address space if the source does not have the private flag set. A
 * page source has a backend behind it that is used to actually get pages. This
 * backend can be a cache, physical memory, etc.
 *
 * A page source backend has 2 main operations: Get and Release. The Get
 * operation is used to get a page for a region when a fault occurs on it.
 * The Release operation is used to signal that a page has been unmapped from
 * a region. It is passed the offset of the page into the source rather than
 * a pointer to the page structure itself. This is to prevent the need for
 * regions to track pages that have been mapped into them. It is up to the
 * backend to find the page corresponding to the offset and decrease its
 * reference count or whatever it needs to do. A page source backend also
 * has a few other operations, for example the Map operation that is called
 * whenever a source using the backend is mapped into an address space, to
 * ensure that the protection flags are valid, etc.
 *
 * An address space is a higher-level system built on top of a page map. The
 * page map is used to perform the actual mapping of virtual addresses to
 * physical addresses provided by the various address space backends.
 */

#include <arch/memmap.h>

#include <cpu/intr.h>

#include <io/vfs.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/cache.h>
#include <mm/malloc.h>
#include <mm/pmm.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/tlb.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>

#if CONFIG_ASPACE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab caches used for address space-related structures. */
static slab_cache_t *aspace_cache;		/**< Cache of address space structures. */
static slab_cache_t *aspace_region_cache;	/**< Cache of address space region structures. */
static slab_cache_t *aspace_source_cache;	/**< Cache of page source structures. */

/** Constructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int aspace_cache_ctor(void *obj, void *data, int kmflag) {
	aspace_t *aspace = (aspace_t *)obj;

	mutex_init(&aspace->lock, "aspace_lock", 0);
	refcount_set(&aspace->count, 0);
	avl_tree_init(&aspace->regions);

	return 0;
}

/** Destructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void aspace_cache_dtor(void *obj, void *data) {
	aspace_t *aspace __unused = (aspace_t *)obj;

	assert(avl_tree_empty(&aspace->regions));
}

/** Allocate a new address space source structure.
 * @param name		Name for the source.
 * @param flags		Behaviour flags for the source.
 * @param mmflag	Allocation flags to use.
 * @return		Pointer to source structure. */
static aspace_source_t *aspace_source_alloc(const char *name, int flags, int mmflag) {
	aspace_source_t *source;

	assert(name);

	source = slab_cache_alloc(aspace_source_cache, mmflag);
	if(!source) {
		return NULL;
	}

	source->name = kstrdup(name, mmflag);
	if(!source->name) {
		slab_cache_free(aspace_source_cache, source);
		return NULL;
	}

	refcount_set(&source->count, 0);
	source->flags = flags;
	return source;
}

/** Destroy an address space source structure.
 * @note		The reference count should be 0.
 * @param source	Source to destroy. */
static void aspace_source_destroy(aspace_source_t *source) {
	assert(refcount_get(&source->count) == 0);

	if(source->backend->destroy) {
		source->backend->destroy(source);
	}
	kfree(source->name);
	slab_cache_free(aspace_source_cache, source);
}

/** Convert region flags to page map flags.
 * @param flags		Flags to convert.
 * @return		Page map flags. */
static inline int aspace_region_flags_to_page(int flags) {
	int ret = 0;

	ret |= ((flags & ASPACE_REGION_READ) ? PAGE_MAP_READ : 0);
	ret |= ((flags & ASPACE_REGION_WRITE) ? PAGE_MAP_WRITE : 0);
	ret |= ((flags & ASPACE_REGION_EXEC) ? PAGE_MAP_EXEC : 0);

	return ret;
}

/** Allocate a new address space region structure.
 * @param start		Start address of the region.
 * @param end		End address of the region.
 * @param flags		Flags for the region.
 * @param source	Page source for the region.
 * @param offset	Offset into the source.
 * @return		Pointer to region structure. */
static aspace_region_t *aspace_region_alloc(ptr_t start, ptr_t end, int flags, aspace_source_t *source, offset_t offset) {
	aspace_region_t *region = slab_cache_alloc(aspace_region_cache, MM_SLEEP);

	region->start = start;
	region->end = end;
	region->flags = flags;
	region->source = source;
	region->offset = offset;

	return region;
}

/** Get the next region in the region list.
 * @param region	Current region.
 * @return		Pointer to next region. */
static aspace_region_t *aspace_region_next(aspace_region_t *region) {
	avl_tree_node_t *node;

	node = avl_tree_node_next(region->node);
	if(node == NULL) {
		return NULL;
	}

	return avl_tree_entry(node, aspace_region_t);
}

/** Searches for a region containing a certain address.
 * @param as		Address space to search in.
 * @param addr		Address to search for.
 * @param nearp		If non-NULL, will be set to the first region higher
 *			than the address if no exact region match is found.
 * @return		Pointer to region if found, false if not. */
static aspace_region_t *aspace_region_find(aspace_t *as, ptr_t addr, aspace_region_t **nearp) {
	avl_tree_node_t *node, *near = NULL;
	aspace_region_t *region;

	if(as->find_cache && as->find_cache->start <= addr && as->find_cache->end > addr) {
		/* Use the cached pointer if it matches. Caching the last found
		 * region helps mainly for page fault handling when code is
		 * hitting different parts of a newly mapped region in
		 * succession. */
		return as->find_cache;
	} else {
		/* Fall back on searching through the AVL tree. */
		node = as->regions.root;
		while(node) {
			region = avl_tree_entry(node, aspace_region_t);
			if(addr >= region->start) {
				if(addr < region->end) {
					as->find_cache = region;
					return region;
				}
				node = node->right;
			} else {
				/* Save this node so that we can find the
				 * next region upon failure. */
				near = node;
				node = node->left;
			}
		}

		if(nearp) {
			*nearp = (near) ? avl_tree_entry(near, aspace_region_t) : NULL;
		}
		return NULL;
	}
}

/** Insert a region into an address space.
 * @note		There should be a hole in the address space for the
 *			region - this will not create one, or check if there
 *			actually is one.
 * @param as		Address space to insert into.
 * @param region	Region to insert. */
static void aspace_region_insert(aspace_t *as, aspace_region_t *region) {
	avl_tree_insert(&as->regions, region->start, region, &region->node);
}

/** Unmap pages covering part or all of a region.
 * @param as		Address space to unmap from.
 * @param region	Region being unmapped.
 * @param start		Start of range to unmap.
 * @param end		End of range to unmap. */
static void aspace_region_unmap(aspace_t *as, aspace_region_t *region, ptr_t start, ptr_t end) {
	offset_t offset;
	ptr_t addr;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(!(region->flags & ASPACE_REGION_RESERVED));
	assert(start < end);
	assert(start >= region->start);
	assert(end <= region->end);

	for(addr = start; addr < end; addr += PAGE_SIZE) {
		if(!page_map_remove(&as->pmap, addr, NULL)) {
			continue;
		}

		/* Release the page just unmapped. */
		offset = (offset_t)(addr - region->start) + region->offset;
		region->source->backend->release(region->source, offset);
	}

	/* Invalidate the necessary TLB entries on all CPUs using the address
	 * space, and drop the page map lock. */
	tlb_invalidate(as, start, end);
}

/** Resize a region. Cannot decrease the start address or increase end address.
 * @param as		Address space region belongs to.
 * @param region	Region being resized.
 * @param start		New start address.
 * @param end		New end address. */
static void aspace_region_resize(aspace_t *as, aspace_region_t *region, ptr_t start, ptr_t end) {
	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(start >= region->start);
	assert(end <= region->end);

	if(!(region->flags & ASPACE_REGION_RESERVED)) {
		if(start - region->start) {
			aspace_region_unmap(as, region, region->start, start);
		}
		if(region->end - end) {
			aspace_region_unmap(as, region, end, region->end);
		}
		region->offset += (start - region->start);
	}

	if(start != region->start) {
		/* Remove the region because we're changing the key. */
		avl_tree_remove(&as->regions, (key_t)region->start);

		region->start = start;
		region->end = end;

		/* Reinsert with the new key. */
		avl_tree_insert(&as->regions, (key_t)region->start, region, &region->node);
	} else {
		region->start = start;
		region->end = end;
	}
}

/** Split a region.
 * @param as		Address space region belongs to.
 * @param region	Region to split.
 * @param end		Where to end first half of region.
 * @param start		Where to start second part of region. */
static void aspace_region_split(aspace_t *as, aspace_region_t *region, ptr_t end, ptr_t start) {
	aspace_region_t *split;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(end > region->start && end < region->end);
	assert(start >= end && start < region->end);

	/* Add another reference to the source because we have another region
	 * using it. */
	refcount_inc(&region->source->count);

	/* Create the split region. */
	split = aspace_region_alloc(start, region->end, region->flags, region->source,
	                             region->offset + (start - region->start));

	/* Unmap the gap between the regions if necessary. */
	if(end != start) {
		aspace_region_unmap(as, region, end, start);
	}

	region->end = end;

	/* Insert the split region. */
	avl_tree_insert(&as->regions, (key_t)split->start, split, &split->node);
}

/** Destroy a region.
 * @param as		Address space region belongs to.
 * @param region	Region to destroy. */
static void aspace_region_destroy(aspace_t *as, aspace_region_t *region) {
	if(!(region->flags & ASPACE_REGION_RESERVED)) {
		aspace_region_unmap(as, region, region->start, region->end);
	}

	avl_tree_remove(&as->regions, region->start);

	if(region->source && refcount_dec(&region->source->count) == 0) {
		aspace_source_destroy(region->source);
	}

	if(region == as->find_cache) {
		as->find_cache = NULL;
	}
	slab_cache_free(aspace_region_cache, region);
}

/** Free a region in an address space.
 * @param as		Address space to free in.
 * @param start		Start of region to free.
 * @param end		End of region to free. */
static void aspace_do_free(aspace_t *as, ptr_t start, ptr_t end) {
	aspace_region_t *region, *near, *next;

	/* Find the start region. */
	region = aspace_region_find(as, start, &near);
	if(region == NULL) {
		if(near == NULL) {
			/* No region matches, and there is not a region after.
			 * Nothing to do. */
			return;
		} else if(near->start >= end) {
			/* Region following does not overlap the region we're
			 * freeing, do nothing. */
			return;
		}

		/* We need to free some regions following us, fall through. */
		region = near;
	} else if(region->start < start) {
		if(region->end == end) {
			/* Just shrink the region and finish. */
			aspace_region_resize(as, region, region->start, start);
			return;
		} else if(region->end < end) {
			/* Shrink the region, move to next and fall through. */
			aspace_region_resize(as, region, region->start, start);

			region = aspace_region_next(region);
			if(region == NULL) {
				return;
			}
		} else {
			/* Split the region and finish. */
			aspace_region_split(as, region, start, end);
			return;
		}
	}

	assert(region->start >= start);

	/* Loop through and eat up all the regions necessary. */
	while(region && region->start < end) {
		if(region->end <= end) {
			/* Completely overlap this region, remove. */
			next = aspace_region_next(region);
			aspace_region_destroy(as, region);
			region = next;
		} else if(region->end > end) {
			/* Resize the existing region. */
			aspace_region_resize(as, region, end, region->end);
			return;
		}
	}
}

/** Searches for free space in an address space.
 * @param as		Address space to search in.
 * @param size		Size of space required.
 * @param addrp		Where to store address of space.
 * @return		True if space found, false if not. */
static bool aspace_find_free(aspace_t *as, size_t size, ptr_t *addrp) {
	aspace_region_t *region, *prev = NULL;
	avl_tree_node_t *node;

	assert(size);

	/* Handle case of address space being empty. */
	if(unlikely(avl_tree_empty(&as->regions))) {
		if(size > ASPACE_SIZE) {
			return false;
		}

		*addrp = ASPACE_BASE;
		return true;
	}

	/* Iterate over all regions in order to find the first suitable hole. */
	for(node = avl_tree_node_first(&as->regions); ; node = avl_tree_node_next(node)) {
		region = avl_tree_entry(node, aspace_region_t);
		if(region == NULL) {
			/* Reached the end of the address space, see if we
			 * have space following the previous entry. */
			assert(prev);
			if((prev->end + size) > prev->end &&
			   (prev->end + size) <= (ASPACE_BASE + ASPACE_SIZE)) {
				/* We have some space, return it. */
				*addrp = prev->end;
				return true;
			} else {
				return false;
			}
		} else if(prev == NULL) {
			/* First region, check if there is a hole preceding it
			 * and whether it is big enough. */
			if((ASPACE_BASE + size) <= region->start) {
				*addrp = ASPACE_BASE;
				return true;
			}
		} else {
			/* Check if there is a gap between the last region
			 * and this region that's big enough. */
			if((region->start - prev->end) >= size) {
				*addrp = prev->end;
				return true;
			}
		}

		prev = region;
	}

	return false;
}

/** Perform the actual work of mapping a region.
 * @param as		Address space to map into.
 * @param start		Start address to map at (if not ASPACE_MAP_FIXED).
 * @param size		Size of the region to map.
 * @param flags		Mapping behaviour flags.
 * @param source	Source backing the region.
 * @param offset	Offset into the source the region should start at.
 * @param addrp		Where to store allocated address (if not
 *			ASPACE_MAP_FIXED).
 * @return		0 on success, negative error code on failure. */
static int aspace_do_map(aspace_t *as, ptr_t start, size_t size, int flags,
                         aspace_source_t *source, offset_t offset, ptr_t *addrp) {
	aspace_region_t *region;
	int ret, rflags;

	assert(source);

	/* Check arguments. */
	if(flags & ASPACE_MAP_FIXED) {
		if(start % PAGE_SIZE || size % PAGE_SIZE) {
			return -ERR_PARAM_INVAL;
		} else if(!size || !aspace_region_fits(start, size)) {
			return -ERR_PARAM_INVAL;
		}
	} else {
		if(!size || !addrp || size % PAGE_SIZE) {
			return -ERR_PARAM_INVAL;
		}
	}

	/* Convert mapping flags to region flags. */
	rflags = flags & (ASPACE_MAP_READ | ASPACE_MAP_WRITE | ASPACE_MAP_EXEC);

	/* Check if the source allows what we've been given. */
	if(source->backend->map) {
		ret = source->backend->map(source, offset, size, rflags);
		if(ret != 0) {
			return ret;
		}
	}

	/* If allocating space, we must now find some. Otherwise, we free up
	 * anything in the location we want to insert to. */
	if(!(flags & ASPACE_MAP_FIXED)) {
		if(!aspace_find_free(as, size, &start)) {
			return -ERR_NO_MEMORY;
		}

		*addrp = start;
	} else {
		aspace_do_free(as, start, start + size);
	}

	/* Create the region structure and insert it. */
	region = aspace_region_alloc(start, start + size, rflags, source, offset);
	aspace_region_insert(as, region);

	/* Place a reference on the source to show we're using it. */
	refcount_inc(&source->count);

	dprintf("aspace: mapped region [%p,%p) (as: %p, source: %s, flags(m/r): %d/%d)\n",
	        region->start, region->end, as, source->name, flags, rflags);
	return 0;
}

#if 0
# pragma mark Anonymous backend.
#endif

/** Get a missing page from an anonymous cache.
 * @param cache		Cache to get page from.
 * @param offset	Offset of page in data source.
 * @param addrp		Where to store address of page obtained.
 * @return		0 on success, negative error code on failure. */
static int aspace_anon_cache_get_page(cache_t *cache, offset_t offset, phys_ptr_t *addrp) {
	*addrp = pmm_alloc(1, MM_SLEEP | PM_ZERO);
	return 0;
}

/** Free a page from an anonymouse cache.
 * @param cache		Cache that the page is in.
 * @param page		Address of page to free.
 * @param offset	Offset of page in data source. */
static void aspace_anon_cache_free_page(cache_t *cache, phys_ptr_t page, offset_t offset) {
	pmm_free(page, 1);
}

/** Anonymous page cache operations. */
static cache_ops_t aspace_anon_cache_ops = {
	.get_page = aspace_anon_cache_get_page,
	.free_page = aspace_anon_cache_free_page,
};

/** Get a page from an anonymous source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @param addrp		Where to store address of page.
 * @return		0 on success, negative error code on failure. */
static int aspace_anon_get(aspace_source_t *source, offset_t offset, phys_ptr_t *addrp) {
	return cache_get(source->data, offset, addrp);
}

/** Release a page in an anonymous source.
 * @param source	Source to release page in.
 * @param offset	Offset into the source.
 * @return		Pointer to page allocated, or NULL on failure. */
static void aspace_anon_release(aspace_source_t *source, offset_t offset) {
	cache_release(source->data, offset, true);
}

/** Destroy data in an anonymous source.
 * @param source	Source to destroy. */
static void aspace_anon_destroy(aspace_source_t *source) {
	if(cache_destroy(source->data) != 0) {
		/* Shouldn't happen as we don't do any page flushing. */
		fatal("Failed to destroy anonymous cache");
	}
}

/** Anonymous address space backend structure. */
static aspace_backend_t aspace_anon_backend = {
	.get = aspace_anon_get,
	.release = aspace_anon_release,
	.destroy = aspace_anon_destroy,
};

#if 0
# pragma mark VFS backends.
#endif

/** Get a page from a VFS source.
 * @param source	Source to get page from.
 * @param offset	Offset into the source.
 * @param addrp		Where to store address of page.
 * @return		0 on success, negative error code on failure. */
static int aspace_file_get(aspace_source_t *source, offset_t offset, phys_ptr_t *addrp) {
	return cache_get(source->data, offset, addrp);
}

/** Release a page in a VFS source.
 * @param source	Source to release page in.
 * @param offset	Offset into the source.
 * @return		Pointer to page allocated, or NULL on failure. */
static void aspace_file_release(aspace_source_t *source, offset_t offset) {
	cache_release(source->data, offset, true);
}

/** Destroy a VFS source.
 * @param source	Source to destroy. */
static void aspace_file_destroy(aspace_source_t *source) {
	vfs_file_cache_release(source->data);
}

/** VFS private address space backend structure. */
static aspace_backend_t aspace_file_private_backend = {
	.get = aspace_file_get,
	.release = aspace_file_release,
	.destroy = aspace_file_destroy,
};

/** Check whether the source can be mapped using the given parameters.
 * @param source	Source being mapped.
 * @param offset	Offset of the mapping in the source.
 * @param size		Size of the mapping.
 * @param flags		Flags the mapping is being created with.
 * @return		0 if mapping allowed, negative error code explaining
 *			why it is not allowed if not. */
static int aspace_file_shared_map(aspace_source_t *source, offset_t offset, size_t size, int flags) {
	vfs_node_t *node = ((cache_t *)source->data)->data;

	/* Shared sources can only be mapped as writeable if the underlying
	 * file is writeable. For private sources it is OK to write read-only
	 * files, because modifications don't go back to the file. */
	return (flags & ASPACE_MAP_WRITE && VFS_NODE_IS_RDONLY(node)) ? -ERR_READ_ONLY : 0;
}

/** VFS shared address space backend structure. */
static aspace_backend_t aspace_file_shared_backend = {
	.get = aspace_file_get,
	.release = aspace_file_release,
	.destroy = aspace_file_destroy,

	.map = aspace_file_shared_map,
};

#if 0
# pragma mark Public interface.
#endif

/** Mark a region as reserved.
 *
 * Marks a region of memory in an address space as reserved. Reserved regions
 * will never be allocated from if mapping without ASPACE_MAP_FIXED, but they
 * can be overwritten with ASPACE_MAP_FIXED mappings or removed byusing
 * aspace_unmap() on the region.
 *
 * @param as		Address space to reserve in.
 * @param start		Start of region to reserve.
 * @param size		Size of region to reserve.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_reserve(aspace_t *as, ptr_t start, size_t size) {
	aspace_region_t *region;

	if(start % PAGE_SIZE || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	} else if(size == 0 || !aspace_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	/* Allocate the region structure. */
	region = aspace_region_alloc(start, start + size, ASPACE_REGION_RESERVED, NULL, 0);

	/* Insert it into the address space. */
	mutex_lock(&as->lock, 0);
	aspace_region_insert(as, region);
	mutex_unlock(&as->lock);

	return 0;
}

/** Map a region of anonymous memory.
 *
 * Maps a region of anonymous memory (i.e. not backed by any data source) into
 * an address space. If the ASPACE_MAP_FIXED flag is specified, then the region
 * will be mapped at the location specified. Otherwise, a region of unused
 * space will be allocated for the mapping.
 *
 * @param as		Address space to map in.
 * @param start		Start address of region (if ASPACE_MAP_FIXED).
 * @param size		Size of region to map (multiple of page size).
 * @param flags		Flags to control mapping behaviour (ASPACE_MAP_*).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_map_anon(aspace_t *as, ptr_t start, size_t size, int flags, ptr_t *addrp) {
	aspace_source_t *source;
	int ret, sflags;

	/* Create the source and cache for the region. */
	sflags = (flags & ASPACE_MAP_PRIVATE) ? ASPACE_SOURCE_PRIVATE : 0;
	source = aspace_source_alloc("[anon]", sflags, MM_SLEEP);
	source->backend = &aspace_anon_backend;
	source->data = cache_create(&aspace_anon_cache_ops, NULL);

	mutex_lock(&as->lock, 0);

	/* Attempt to map the region in. */
	ret = aspace_do_map(as, start, size, flags, source, 0, addrp);
	if(ret != 0) {
		aspace_source_destroy(source);
	}

	mutex_unlock(&as->lock);
	return ret;
}

/** Map a file into an address space.
 *
 * Maps part of a file into an address space. If the ASPACE_MAP_FIXED flag is
 * specified, then the region will be mapped at the location specified.
 * Otherwise, a region of unused space will be allocated for the mapping. If
 * the ASPACE_MAP_PRIVATE flag is specified, then changes made to the mapped
 * data will not be made in the underlying file, and will not be visible to
 * other regions mapping the file. Also, changes made to the file's data after
 * the mapping has been accessing it may not be visible in the mapping. If the
 * ASPACE_MAP_PRIVATE flag is not specified, then changes to the mapped data
 * will be made in the underlying file, and will be visible to other regions
 * mapping the file.
 *
 * @param as		Address space to map in.
 * @param start		Start address of region (if ASPACE_MAP_FIXED).
 * @param size		Size of region to map (multiple of page size).
 * @param flags		Flags to control mapping behaviour (ASPACE_MAP_*).
 * @param node		Filesystem node to map. Must be a file.
 * @param offset	Offset in the file to start mapping at (multiple of
 *			page size).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_map_file(aspace_t *as, ptr_t start, size_t size, int flags, vfs_node_t *node, offset_t offset, ptr_t *addrp) {
	aspace_source_t *source;
	int ret;

	/* Create the source using the correct backend. */
	if(flags & ASPACE_MAP_PRIVATE) {
		/* TODO: Name. */
		source = aspace_source_alloc("[file]", ASPACE_SOURCE_PRIVATE, MM_SLEEP);
		source->backend = &aspace_file_private_backend;

		ret = vfs_file_cache_get(node, true, (cache_t **)&source->data);
		if(ret != 0) {
			kfree(source->name);
			slab_cache_free(aspace_source_cache, source);
			return ret;
		}
	} else {
		source = aspace_source_alloc("[file]", 0, MM_SLEEP);
		source->backend = &aspace_file_shared_backend;

		ret = vfs_file_cache_get(node, false, (cache_t **)&source->data);
		if(ret != 0) {
			kfree(source->name);
			slab_cache_free(aspace_source_cache, source);
			return ret;
		}
	}

	mutex_lock(&as->lock, 0);

	/* Attempt to map the region in. */
	ret = aspace_do_map(as, start, size, flags, source, 0, addrp);
	if(ret != 0) {
		aspace_source_destroy(source);
	}

	mutex_unlock(&as->lock);
	return ret;
}

/** Unmaps a region in an address space.
 *
 * Marks the specified address range in an address space as free and unmaps all
 * pages that may be mapped there.
 *
 * @param as		Address space to free in.
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_unmap(aspace_t *as, ptr_t start, size_t size) {
	if(start % PAGE_SIZE || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	} else if(size == 0 || !aspace_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&as->lock, 0);

	/* Only bother doing anything if the tree isn't empty. */
	if(likely(!avl_tree_empty(&as->regions))) {
		aspace_do_free(as, start, start + size);
	}

	dprintf("aspace: freed region [%p,%p) (as: %p)\n", start, start + size, as);
	mutex_unlock(&as->lock);
	return 0;
}

/** Handle a page fault.
 *
 * Handles a page fault on the current address space by attempting to map in
 * a page.
 *
 * @param addr		Address that the fault occurred at.
 * @param reason	Reason for the fault.
 * @param access	Type of access that was made at the address.
 *
 * @return		PF_STATUS_OK if handled, other status if not.
 */
int aspace_pagefault(ptr_t addr, int reason, int access) {
	aspace_t *as = curr_aspace;
	aspace_region_t *region;
	phys_ptr_t page;
	offset_t offset;
	int ret;

	/* If we don't currently have an address space then we can't handle
	 * anything... */
	if(as == NULL) {
		return PF_STATUS_FAULT;
	}

	/* TODO: COW. */
	if(reason == PF_REASON_PROT) {
		return PF_STATUS_FAULT;
	}

	/* Safe to take the lock despite us being in an interrupt - the lock
	 * is only held within the functions in this file, and they should not
	 * incur a pagefault (if they do there's something wrong!). */
	mutex_lock(&as->lock, 0);

	/* Find the region that the fault occurred in - if its a reserved
	 * region, the memory is unmapped so treat it as though no region is
	 * there. */
	if(unlikely(!(region = aspace_region_find(as, addr, NULL)))) {
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	} else if(unlikely(region->flags & ASPACE_REGION_RESERVED)) {
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	assert(region->source);
	assert(region->source->backend->get);

	/* Check protection flags. */
	if((access == PF_ACCESS_READ && !(region->flags & ASPACE_REGION_READ)) ||
	   (access == PF_ACCESS_WRITE && !(region->flags & ASPACE_REGION_WRITE)) ||
	   (access == PF_ACCESS_EXEC && !(region->flags & ASPACE_REGION_EXEC))) {
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	/* Work out the offset to pass into the fault handler. */
	offset = (offset_t)((addr & PAGE_MASK) - region->start) + region->offset;

	/* Get the page from the source. */
	ret = region->source->backend->get(region->source, offset, &page);
	if(unlikely(ret != 0)) {
		dprintf("aspace: failed to get page for %p in %p: %d\n", addr, as, ret);
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	/* Map the page in to the address space. */
	if(!page_map_insert(&as->pmap, (addr & PAGE_MASK), page, aspace_region_flags_to_page(region->flags), MM_SLEEP)) {
		region->source->backend->release(region->source, offset);

		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	mutex_unlock(&as->lock);
	dprintf("aspace: fault at %p in %p: 0x%" PRIpp " -> %p\n",
		addr, as, page, (addr & PAGE_MASK));
	return PF_STATUS_OK;
}

/** Switch to another address space.
 *
 * Switches to a different address space. Does not take address space lock
 * because this function is used during rescheduling.
 *
 * @param as		Address space to switch to (if NULL, then will switch
 *			to the kernel address space).
 */
void aspace_switch(aspace_t *as) {
	bool state = intr_disable();

	/* Decrease reference count on the old address space if there is one. */
	if(curr_aspace) {
		refcount_dec(&curr_aspace->count);
	}

	/* If NULL, switch to kernel address space. */
	if(as) {
		refcount_inc(&as->count);
		page_map_switch(&as->pmap);
	} else {
		page_map_switch(&kernel_page_map);
	}

	curr_aspace = as;

	intr_restore(state);
}

/** Create a new address space.
 *
 * Allocates a new address space structure and initializes it.
 *
 * @return		Pointer to address space structure, NULL on failure.
 */
aspace_t *aspace_create(void) {
	aspace_t *as;

	as = slab_cache_alloc(aspace_cache, MM_SLEEP);
	if(page_map_init(&as->pmap) != 0) {
		slab_cache_free(aspace_cache, as);
		return NULL;
	}

	as->find_cache = NULL;

	/* Do architecture-specific initialization. */
	if(aspace_arch_create(as) != 0) {
		page_map_destroy(&as->pmap);
		slab_cache_free(aspace_cache, as);
		return NULL;
	}

	return as;
}

/** Destroy an address space.
 *
 * Removes all memory mappings in an address space and frees it. This must
 * not be called if the address space is in use on any CPU. There should also
 * be no references to it in any processes, to ensure that nothing will attempt
 * to access it while it is being destroyed.
 *
 * @param as		Address space to destroy.
 */
void aspace_destroy(aspace_t *as) {
	avl_tree_node_t *node;

	assert(as);

	if(refcount_get(&as->count) > 0) {
		fatal("Destroying in-use address space");
	}

	/* Unmap and destroy each region. Do not use the AVL tree iterator
	 * here as it is not safe to do so when modifying the tree. */
	while((node = avl_tree_node_first(&as->regions))) {
		aspace_region_destroy(as, avl_tree_entry(node, aspace_region_t));
	}

	/* Destroy the page map. */
	page_map_destroy(&as->pmap);

	slab_cache_free(aspace_cache, as);
}

/** Initialize the address space caches. */
void aspace_init(void) {
	aspace_cache = slab_cache_create("aspace_cache", sizeof(aspace_t), 0,
	                                 aspace_cache_ctor, aspace_cache_dtor,
	                                 NULL, NULL, NULL, 0, MM_FATAL);
	aspace_region_cache = slab_cache_create("aspace_region_cache", sizeof(aspace_region_t), 0,
	                                        NULL, NULL, NULL, NULL, NULL, 0, MM_FATAL);
	aspace_source_cache = slab_cache_create("aspace_source_cache", sizeof(aspace_source_t), 0,
	                                        NULL, NULL, NULL, NULL, NULL, 0, MM_FATAL);
}

#if 0
# pragma mark Debugger commands.
#endif

/** Dump an address space.
 *
 * Dumps out a list of all regions held in an address space.
 *
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 *
 * @return		KDBG_OK on success, KDBG_FAIL on failure.
 */
int kdbg_cmd_aspace(int argc, char **argv) {
	aspace_region_t *region;
	unative_t addr;
	aspace_t *as;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s <address>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints the contents of the address space at the specified address. This address\n");
		kprintf(LOG_NONE, "is given as an expression.\n");
		return KDBG_OK;
	} else if(argc != 2) {
		kprintf(LOG_NONE, "Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(kdbg_parse_expression(argv[1], &addr, NULL) != KDBG_OK) {
		return KDBG_FAIL;
	}

	as = (aspace_t *)addr;

	kprintf(LOG_NONE, "Base               End                Flags Source\n");
	kprintf(LOG_NONE, "====               ===                ===== ======\n");

	AVL_TREE_FOREACH(&as->regions, iter) {
		region = avl_tree_entry(iter, aspace_region_t);

		kprintf(LOG_NONE, "%-18p %-18p %-5d %p+%" PRIo ": %s\n",
		        region->start, region->end, region->flags,
		        region->source, region->offset,
			(region->source) ? region->source->name : "");
	}

	return KDBG_OK;
}

#if 0
# pragma mark System calls.
#endif

/** Map a region of anonymous memory.
 *
 * Maps a region of anonymous memory (i.e. not backed by any data source) into
 * the calling process' address space. If the ASPACE_MAP_FIXED flag is
 * specified, then the region will be mapped at the location specified.
 * Otherwise, a region of unused space will be allocated for the mapping.
 *
 * @param start		Start address of region (if ASPACE_MAP_FIXED).
 * @param size		Size of region to map (multiple of page size).
 * @param flags		Flags to control mapping behaviour (ASPACE_MAP_*).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_aspace_map_anon(void *start, size_t size, int flags, void **addrp) {
	ptr_t addr;
	int ret;

	ret = aspace_map_anon(curr_proc->aspace, (ptr_t)start, size, flags, &addr);
	if(ret != 0) {
		return ret;
	}

	/* TODO: Better write functions for integer values. */
	ret = memcpy_to_user(addrp, &addr, sizeof(void *));
	if(ret != 0) {
		return ret;
	}

	return 0;
}

/** Map a file into memory.
 *
 * Maps part of a file into the calling process' address space. If the
 * ASPACE_MAP_FIXED flag is specified, then the region will be mapped at the
 * location specified. Otherwise, a region of unused space will be allocated
 * for the mapping. If the ASPACE_MAP_PRIVATE flag is specified, then changes
 * made to the mapped data will not be made in the underlying file, and will
 * not be visible to other regions mapping the file. Also, changes made to the
 * file's data after the mapping has been accessing it may not be visible in
 * the mapping. If the ASPACE_MAP_PRIVATE flag is not specified, then changes
 * to the mapped data will be made in the underlying file, and will be visible
 * to other regions mapping the file.
 *
 * @param args		Pointer to arguments structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_aspace_map_file(aspace_map_file_args_t *args) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Unmaps a region of memory.
 *
 * Marks the specified address range in the calling process' address space as
 * free and unmaps all pages that may be mapped there.
 *
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_aspace_unmap(void *start, size_t size) {
	return aspace_unmap(curr_proc->aspace, (ptr_t)start, size);
}
