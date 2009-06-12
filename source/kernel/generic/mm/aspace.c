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
 * At the top level, there is collection of mapped/reserved memory regions.
 * These represent the memory mappings within the address space. Underneath
 * a mapped region is a page source. A page source can be shared between
 * mulitple regions (sharing occurs when a region has to be split for whatever
 * reason). It can also be shared across address spaces when cloning an address
 * space if a mapping using it is mapped as shared. A page source has a backend
 * behind it that is used to actually get pages. This backend can be a cache,
 * physical memory, etc. The backend part was introduced rather than using a
 * cache directly to facilitate physical memory mappings - use of a cache here
 * would be pointless.
 *
 * A page source backend has 2 main operations: Get and Release. The Get
 * operation is used to get a page for a region when a fault occurs on it.
 * The Release operation is used to signal that a page has been unmapped from
 * a region. It is passed the offset of the page into the source rather than
 * a pointer to the page structure itself. This is to prevent the need for
 * regions to track pages that have been mapped into them. It is up to the
 * backend to find the page corresponding to the offset and decrease its
 * reference count or whatever it needs to do.
 *
 * An address space is a higher-level system built on top of a page map. The
 * page map is used to perform the actual mapping of virtual addresses to
 * physical address provided by the various address space backends. It also
 * handles the TLB, so none of this code has to deal with maintaining TLB
 * consistency.
 */

#include <arch/memmap.h>

#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/slab.h>
#include <mm/tlb.h>

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

	mutex_init(&aspace->lock, "aspace_lock");
	refcount_set(&aspace->count, 0);
	avltree_init(&aspace->regions);

	return 0;
}

/** Destructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void aspace_cache_dtor(void *obj, void *data) {
	aspace_t *aspace __unused = (aspace_t *)obj;

	assert(avltree_empty(&aspace->regions));
}

/** Convert region flags to page map flags.
 * @param flags		Flags to convert.
 * @return		Page map flags. */
static int aspace_flags_to_page(int flags) {
	int ret = 0;

	ret |= ((flags & AS_REGION_READ) ? PAGE_MAP_READ : 0);
	ret |= ((flags & AS_REGION_WRITE) ? PAGE_MAP_WRITE : 0);
	ret |= ((flags & AS_REGION_EXEC) ? PAGE_MAP_EXEC : 0);

	return ret;
}

/** Create a new address space region structure.
 * @param flags		Flags for the region.
 * @param source	Page source for the region.
 * @return		Pointer to region structure. */
static aspace_region_t *aspace_region_create(int flags, aspace_source_t *source) {
	aspace_region_t *region = slab_cache_alloc(aspace_region_cache, MM_SLEEP);

	region->flags = flags;
	region->source = source;
	region->offset = 0;

	return region;
}

/** Get the next region in the region list.
 * @param region	Current region.
 * @return		Pointer to next region. */
static aspace_region_t *aspace_region_next(aspace_region_t *region) {
	avltree_node_t *node;

	node = avltree_node_next(region->node);
	if(node == NULL) {
		return NULL;
	}

	return avltree_entry(node, aspace_region_t);
}

/** Searches for a region containing a certain address.
 * @param as		Address space to search in.
 * @param addr		Address to search for.
 * @param nearp		If non-NULL, will be set to the first region higher
 *			than the address if no exact region match is found.
 * @return		Pointer to region if found, false if not. */
static aspace_region_t *aspace_region_find(aspace_t *as, ptr_t addr, aspace_region_t **nearp) {
	avltree_node_t *node, *near = NULL;
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
			region = avltree_entry(node, aspace_region_t);
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
			*nearp = (near) ? avltree_entry(near, aspace_region_t) : NULL;
		}
		return NULL;
	}
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
	assert(!(region->flags & AS_REGION_RESERVED));
	assert(start < end);
	assert(start >= region->start);
	assert(end <= region->end);

	/* Lock the page map. */
	page_map_lock(&as->pmap, 0);

	/* Begin the TLB shootdown process, and invalidate the range on the
	 * current CPU. */
	tlb_shootdown(as, start, end);

	for(addr = start; addr < end; addr += PAGE_SIZE) {
		if(!page_map_remove(&as->pmap, addr, NULL)) {
			continue;
		}

		/* Release the page just unmapped. */
		offset = (offset_t)(addr - region->start) + region->offset + region->source->offset;
		region->source->backend->release(region->source, offset);
	}

	/* Unlock the page map. This will let other CPUs that were waiting
	 * to perform TLB invalidation continue. */
	page_map_unlock(&as->pmap);
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

	if(!(region->flags & AS_REGION_RESERVED)) {
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
		avltree_remove(&as->regions, (key_t)region->start);

		region->start = start;
		region->end = end;

		/* Reinsert with the new key. */
		avltree_insert(&as->regions, (key_t)region->start, region, &region->node);
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
	split = aspace_region_create(region->flags, region->source);
	split->start = start;
	split->end = region->end; 
	split->offset = region->offset + (start - region->start);

	/* Unmap the gap between the regions if necessary. */
	if(end != start) {
		aspace_region_unmap(as, region, end, start);
	}

	region->end = end;

	/* Insert the split region. */
	avltree_insert(&as->regions, (key_t)split->start, split, &split->node);
}

/** Destroy a region.
 * @param as		Address space region belongs to.
 * @param region	Region to destroy. */
static void aspace_region_destroy(aspace_t *as, aspace_region_t *region) {
	aspace_region_unmap(as, region, region->start, region->end);
	avltree_remove(&as->regions, region->start);

	if(refcount_dec(&region->source->count) == 0) {
		if(region->source->backend->destroy) {
			region->source->backend->destroy(region->source);
		}
		kfree(region->source->name);
		slab_cache_free(aspace_source_cache, region->source);
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
	avltree_node_t *node;

	/* Should only be called if the address space is not empty. */
	assert(!avltree_empty(&as->regions));
	assert(size);

	/* Iterate over all regions in order to find the first suitable hole. */
	for(node = avltree_node_first(&as->regions); ; node = avltree_node_next(node)) {
		region = avltree_entry(node, aspace_region_t);
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

/** Allocate a new address space source structure.
 *
 * Allocates a new address space source structure and initializes parts of
 * it.
 *
 * @param name		Name for the source.
 *
 * @return		Pointer to source.
 */
aspace_source_t *aspace_source_alloc(const char *name) {
	aspace_source_t *source = slab_cache_alloc(aspace_source_cache, MM_SLEEP);

	assert(name);

	source->name = kstrdup(name, MM_SLEEP);
	refcount_set(&source->count, 0);
	return source;
}

/** Allocate space in an address space.
 *
 * Allocates space within an address space and creates a new region with
 * the given backend in it. It is invalid to specify AS_REGION_RESERVED as
 * an argument to this function.
 *
 * @param as		Address space to allocate in.
 * @param size		Size of the region to allocate (must be a multiple of
 *			the system page size).
 * @param flags		Protection/behaviour flags for the region.
 * @param source	Page source to use for the region.
 * @param addrp		Where to store address allocated.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_alloc(aspace_t *as, size_t size, int flags, aspace_source_t *source, ptr_t *addrp) {
	aspace_region_t *region;

	if(flags & AS_REGION_RESERVED || source == NULL || addrp == NULL) {
		return -ERR_PARAM_INVAL;
	} else if(size == 0 || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	}

	/* Create the region structure and fill it in. */
	region = aspace_region_create(flags, source);

	mutex_lock(&as->lock, 0);

	/* Now find a sufficient range of free space for us to place the
	 * region in. If there are no regions in the address space we just
	 * need to check if the region fits and then stick it in to the tree. */
	if(unlikely(avltree_empty(&as->regions))) {
		if(size > ASPACE_SIZE) {
			slab_cache_free(aspace_region_cache, region);
			mutex_unlock(&as->lock);
			return -ERR_NO_MEMORY;
		}

		region->start = ASPACE_BASE;
		region->end = ASPACE_BASE + size;

		avltree_insert(&as->regions, region->start, region, &region->node);
	} else {
		/* Look for a suitable hole for a region. */
		if(!aspace_find_free(as, size, &region->start)) {
			slab_cache_free(aspace_region_cache, region);
			mutex_unlock(&as->lock);
			return -ERR_NO_MEMORY;
		}

		region->end = region->start + size;

		avltree_insert(&as->regions, region->start, region, &region->node);
	}

	/* Place a reference on the source to show we're using it. */
	refcount_inc(&source->count);

	dprintf("aspace: allocated region [0x%p,0x%p) (as: 0x%p)\n", region->start, region->end, as);
	*addrp = region->start;
	mutex_unlock(&as->lock);
	return 0;
}

/** Insert a region into an address space.
 *
 * Creates a new region with the given backend and inserts it into an address
 * space at the location specified. If AS_REGION_RESERVED is specified, a
 * backend is not required.
 *
 * @param as		Address space to insert into.
 * @param start		Address to insert region at (must be a multiple of the
 *			system page size).
 * @param size		Size of the region to create (must be a multiple of the
 *			system page size).
 * @param flags		Protection/behaviour flags for the region.
 * @param source	Page source to use for the region.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_insert(aspace_t *as, ptr_t start, size_t size, int flags, aspace_source_t *source) {
	aspace_region_t *region;

	if(!(flags & AS_REGION_RESERVED) && source == NULL) {
		return -ERR_PARAM_INVAL;
	} else if(start % PAGE_SIZE || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	} else if(size == 0 || !aspace_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	/* Create the region structure and fill it in. */
	region = aspace_region_create(flags, source);
	region->start = start;
	region->end = start + size;

	mutex_lock(&as->lock, 0);

	/* Do the dirty work of inserting the region. */
	if(unlikely(avltree_empty(&as->regions))) {
		/* Empty, just stick the region in as-is. */
		avltree_insert(&as->regions, region->start, region, &region->node);
	} else {
		/* Ok, need to free up space to place this region in. */
		aspace_do_free(as, region->start, region->end);

		/* Now have a free hole for the region, put it in. */
		avltree_insert(&as->regions, region->start, region, &region->node);
	}

	/* Place a reference on the source to show we're using it. */
	if(source != NULL) {
		refcount_inc(&source->count);
	}

	dprintf("aspace: inserted region [0x%p,0x%p) (as: 0x%p)\n", region->start, region->end, as);
	mutex_unlock(&as->lock);
	return 0;
}

/** Marks a region as unused in an address space.
 *
 * Marks the specified address range as free and unmaps all pages that may
 * be mapped there.
 *
 * @param as		Address space to free in.
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		0 on success, negative error code on failure.
 */
int aspace_free(aspace_t *as, ptr_t start, size_t size) {
	if(start % PAGE_SIZE || size % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	} else if(size == 0 || !aspace_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&as->lock, 0);

	/* Only bother doing anything if the tree isn't empty. */
	if(likely(!avltree_empty(&as->regions))) {
		aspace_do_free(as, start, start + size);
	}

	dprintf("aspace: freed region [0x%p,0x%p) (as: 0x%p)\n", start, start + size, as);
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
	} else if(unlikely(region->flags & AS_REGION_RESERVED)) {
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	assert(region->source);
	assert(region->source->backend->get);

	/* Check protection flags. */
	if((access == PF_ACCESS_READ && !(region->flags & AS_REGION_READ)) ||
	   (access == PF_ACCESS_WRITE && !(region->flags & AS_REGION_WRITE)) ||
	   (access == PF_ACCESS_EXEC && !(region->flags & AS_REGION_EXEC))) {
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	/* Work out the offset to pass into the fault handler. */
	offset = (offset_t)((addr & PAGE_MASK) - region->start) + region->offset + region->source->offset;

	/* Get the page from the source. */
	ret = region->source->backend->get(region->source, offset, &page);
	if(unlikely(ret != 0)) {
		dprintf("aspace: failed to get page for 0x%p in 0x%p: %d\n", addr, as, ret);
		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	/* Map the page in to the address space. */
	page_map_lock(&as->pmap, 0);
	if(!page_map_insert(&as->pmap, (addr & PAGE_MASK), page, aspace_flags_to_page(region->flags), MM_SLEEP)) {
		page_map_unlock(&as->pmap);

		region->source->backend->release(region->source, offset);

		mutex_unlock(&as->lock);
		return PF_STATUS_FAULT;
	}

	page_map_unlock(&as->pmap);
	mutex_unlock(&as->lock);
	dprintf("aspace: fault at 0x%p in 0x%p: 0x%" PRIpp " -> 0x%p\n",
		addr, as, page, (addr & PAGE_MASK));
	return PF_STATUS_OK;
}

/** Switch to another address space.
 *
 * Switches to a different address space. Does not take address space lock
 * because it is used during scheduling.
 *
 * @param as		Address space to switch to.
 */
void aspace_switch(aspace_t *as) {
	bool state = intr_disable();

	if(curr_aspace) {
		refcount_dec(&curr_aspace->count);
	}

	refcount_inc(&as->count);
	page_map_switch(&as->pmap);
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
	avltree_node_t *node;

	if(refcount_get(&as->count) > 0) {
		fatal("Destroying in-use address space");
	}

	/* Unmap and destroy each region. Do not use the AVL tree iterator
	 * here as it is not safe to do so when modifying the tree. */
	while((node = avltree_node_first(&as->regions))) {
		aspace_region_destroy(as, avltree_entry(node, aspace_region_t));
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

		kprintf(LOG_NONE, "Dumps out the address space at the specified address. This address\n");
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

	kprintf(LOG_NONE, "Base               End                Flags  Source\n");
	kprintf(LOG_NONE, "====               ===                =====  ======\n");

	AVLTREE_FOREACH(&as->regions, iter) {
		region = avltree_entry(iter, aspace_region_t);

		kprintf(LOG_NONE, "0x%-16p 0x%-16p %-6d 0x%p+%" PRIo " %s\n",
		        region->start, region->end, region->flags,
		        region->source, region->offset,
			(region->source) ? region->source->name : "");
	}

	return KDBG_OK;
}
