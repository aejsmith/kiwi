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
 * @brief		Virtual memory manager.
 *
 * Reference:
 * - The UVM Virtual Memory System.
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.28.1236
 *
 * Parts of the design of the VMM are inspired by NetBSD's UVM (although not
 * the same as), in particular the implementation of anonymous memory and
 * copy-on-write.
 *
 * A brief note about reference counting for pages in the anonymous memory
 * layer.
 *  - The reference count in the page structure is used to track how many
 *    anonymous objects refer to a single page (i.e. object has been duplicated
 *    but the page has not been copied, because no write fault has occurred).
 *    If, when a write fault occurs on a page, the page structure reference
 *    count is greater than 1, the page is copied. Otherwise, the page is just
 *    remapped as read-write (if the region is VM_REGION_WRITE, that is).
 *  - Each object also contains an array of reference counts for each page that
 *    the object can cover. This array is used to track how many regions are
 *    mapping each page of the
 *    object, allowing pages to be freed when no more regions refer to them.
 *
 * @todo		The anonymous object page array could be changed into a
 *			two-level array, which would reduce memory consumption
 *			for large, sparsely-used objects.
 * @todo		Swap support.
 */

#include <arch/memmap.h>

#include <cpu/intr.h>

#include <io/device.h>
#include <io/vfs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/tlb.h>
#include <mm/vm.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <kdbg.h>

#if CONFIG_VM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab caches used for VM structures. */
static slab_cache_t *vm_aspace_cache;
static slab_cache_t *vm_region_cache;
static slab_cache_t *vm_amap_cache;

/** Constructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int vm_aspace_ctor(void *obj, void *data, int kmflag) {
	vm_aspace_t *as = obj;

	mutex_init(&as->lock, "vm_aspace_lock", 0);
	refcount_set(&as->count, 0);
	avl_tree_init(&as->regions);
	return 0;
}

/** Constructor for anonymous map objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int vm_amap_ctor(void *obj, void *data, int kmflag) {
	vm_amap_t *map = obj;

	mutex_init(&map->lock, "vm_amap_lock", 0);
	return 0;
}

/** Create an anonymous map.
 * @param size		Size of object.
 * @return		Pointer to created map (reference count will be 1). */
static vm_amap_t *vm_amap_create(size_t size) {
	vm_amap_t *map;

	assert(size);

	map = slab_cache_alloc(vm_amap_cache, MM_SLEEP);
	refcount_set(&map->count, 1);
	map->curr_size = 0;
	map->max_size = size >> PAGE_WIDTH;
	map->pages = kcalloc(map->max_size, sizeof(vm_page_t *), MM_SLEEP);
	map->rref = kcalloc(map->max_size, sizeof(uint16_t *), MM_SLEEP);
	dprintf("vm: created anonymous map %p (size: %zu, pages: %zu)\n", map, size, map->max_size);
	return map;
}

/** Destroy an anonymous map.
 * @param map		Map to release. */
static void vm_amap_release(vm_amap_t *map) {
	if(refcount_dec(&map->count) == 0) {
		assert(!map->curr_size);

		kfree(map->rref);
		kfree(map->pages);
		dprintf("vm: destroyed anonymous map %p\n", map);
		slab_cache_free(vm_amap_cache, map);
	}
}

/** Increase the region reference count for part of an anonymous map.
 * @param map		Map to increase count on.
 * @param offset	Offset into the map to reference from.
 * @param size		Size of the range to reference.
 * @return		0 on success, negative error code on failure. */
static int vm_amap_map(vm_amap_t *map, offset_t offset, size_t size) {
	size_t i, j, start, end;

	mutex_lock(&map->lock);

	/* Work out the entries within the object that this covers and ensure
	 * it's within the object - for anonymous objects mappings can't be
	 * outside the object. */
	start = (size_t)(offset >> PAGE_WIDTH);
	end = start + (size >> PAGE_WIDTH);
	assert(end <= map->max_size);

	/* Increase the region reference counts for pages in the region. */
	for(i = start; i < end; i++) {
		if(map->rref[i] == UINT16_MAX) {
			kprintf(LOG_DEBUG, "vm: anon object %p rref[%zu] is at maximum value!\n", map, i);

			/* Go and undo what we've done. */
			for(j = start; j < i; j++) {
				map->rref[j]--;
			}
			mutex_unlock(&map->lock);
			return -ERR_RESOURCE_UNAVAIL;
		}
		map->rref[i]++;
	}

	mutex_unlock(&map->lock);
	return 0;
}

/** Check if a region fits in the user memory area.
 * @param start		Start address of region.
 * @param size		Size of region.
 * @return		Whether region fits. */
static inline bool vm_region_fits(ptr_t start, size_t size) {
	ptr_t end = start + size;

	if(end < start || end > (USER_MEMORY_BASE + USER_MEMORY_SIZE)) {
		return false;
#if USER_MEMORY_BASE != 0
	} else if(start < USER_MEMORY_BASE) {
		return false;
#endif
	} else {
		return true;
	}
}

/** Allocate a new region structure. Caller must attach object to it.
 * @param as		Address space of the region.
 * @param start		Start address of the region.
 * @param end		End address of the region.
 * @param flags		Flags for the region.
 * @return		Pointer to region structure. */
static vm_region_t *vm_region_alloc(vm_aspace_t *as, ptr_t start, ptr_t end, int flags) {
	vm_region_t *region = slab_cache_alloc(vm_region_cache, MM_SLEEP);

	region->as = as;
	region->start = start;
	region->end = end;
	region->flags = flags;
	region->handle = NULL;
	region->obj_offset = 0;
	region->amap = NULL;
	region->amap_offset = 0;
	return region;
}

/** Searches for a region containing an address.
 * @param as		Address space to search in (should be locked).
 * @param addr		Address to search for.
 * @param nearp		If non-NULL, will be set to the first region higher
 *			than the address if no exact region match is found.
 * @return		Pointer to region if found, false if not. */
static vm_region_t *vm_region_find(vm_aspace_t *as, ptr_t addr, vm_region_t **nearp) {
	avl_tree_node_t *node, *near = NULL;
	vm_region_t *region;

	/* Check if the cached pointer matches. Caching the last found region
	 * helps mainly for page fault handling when code is hitting different
	 * parts of a newly-mapped region in succession. */
	if(as->find_cache && as->find_cache->start <= addr && as->find_cache->end > addr) {
		return as->find_cache;
	}

	/* Fall back on searching through the AVL tree. */
	node = as->regions.root;
	while(node) {
		region = avl_tree_entry(node, vm_region_t);
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

	/* Failed, save the nearest entry if requested. */
	if(nearp) {
		*nearp = (near) ? avl_tree_entry(near, vm_region_t) : NULL;
	}
	return NULL;
}

/** Get the next region in the region list.
 * @param region	Current region.
 * @return		Pointer to next region. */
static vm_region_t *vm_region_next(vm_region_t *region) {
	avl_tree_node_t *node = avl_tree_node_next(region->node);
	return (node) ? avl_tree_entry(node, vm_region_t) : NULL;
}

/** Release a page that was mapped in a region.
 * @param region	Region that the page was mapped in.
 * @param offset	Offset into the region the page was mapped at.
 * @param paddr		Physical address that was unmapped. */
static void vm_region_page_release(vm_region_t *region, offset_t offset, phys_ptr_t paddr) {
	size_t i;

	if(region->amap) {
		offset += region->amap_offset;
		i = (size_t)(offset >> PAGE_WIDTH);

		assert(i < region->amap->max_size);

		/* If page is in the object, then do nothing. */
		if(region->amap->pages[i]) {
			assert(region->amap->pages[i]->addr == paddr);
			return;
		}

		/* Page must have come from source, release it there. */
		assert(region->handle);
		assert(region->handle->object->type->page_release);

		offset += region->obj_offset;
		region->handle->object->type->page_release(region->handle, offset, paddr);
	} else if(region->handle->object->type->page_release) {
		offset += region->obj_offset;
		region->handle->object->type->page_release(region->handle, offset, paddr);
	}
}

/** Unmap all or part of a region.
 * @note		This function is called whenever part of a region is
 *			going to be removed. It unmaps pages covering the area,
 *			and updates the region's anonymous map (if it has one).
 * @note		Does not release the anonymous map and object if the
 *			entire region is being removed - this is done in
 *			vm_region_destroy() since only that function should be
 *			used to remove an entire region.
 * @param region	Region being unmapped (should not be reserved).
 * @param start		Start of range to unmap.
 * @param end		End of range to unmap. */
static void vm_region_unmap(vm_region_t *region, ptr_t start, ptr_t end) {
	phys_ptr_t paddr;
	offset_t offset;
	ptr_t vaddr;
	size_t i;

	assert(!(region->flags & VM_REGION_RESERVED));
	assert(region->handle || region->amap);

	/* Acquire the anonymous map lock if there is one. */
	if(region->amap) {
		mutex_lock(&region->amap->lock);
	}

	for(vaddr = start; vaddr < end; vaddr += PAGE_SIZE) {
		/* Unmap the page and release it from its source. */
		if(page_map_remove(&region->as->pmap, vaddr, &paddr)) {
			vm_region_page_release(region, (offset_t)vaddr - region->start, paddr);
		}

		/* Update the region reference count on the anonymous map. */
		if(region->amap) {
			offset = (offset_t)(vaddr - region->start) + region->amap_offset;
			i = (size_t)(offset >> PAGE_WIDTH);

			assert(i < region->amap->max_size);
			assert(region->amap->rref[i]);

			if(--region->amap->rref[i] == 0 && region->amap->pages[i]) {
				dprintf("vm: anon object rref %zu reached 0, freeing 0x%" PRIpp " (amap: %p)\n",
				        i, region->amap->pages[i]->addr, region->amap);
				if(refcount_dec(&region->amap->pages[i]->count) == 0) {
					vm_page_free(region->amap->pages[i], 1);
				}
				region->amap->pages[i] = NULL;
				region->amap->curr_size--;
			}
		}
	}

	if(region->amap) {
		mutex_unlock(&region->amap->lock);
	}

	/* Invalidate the TLB entries on all CPUs using the address space. */
	tlb_invalidate(region->as, start, end);
}

/** Shrink a region.
 * @param region	Region being shrunk.
 * @param start		New start address.
 * @param end		New end address. */
static void vm_region_shrink(vm_region_t *region, ptr_t start, ptr_t end) {
	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(start < end);
	assert(start >= region->start);
	assert(end <= region->end);

	/* If not reserved, unmap pages in the areas we're not going to cover
	 * any more, and let the object know that we're doing this. */
	if(!(region->flags & VM_REGION_RESERVED)) {
		if(region->end - end) {
			vm_region_unmap(region, end, region->end);
		}
		if(start - region->start) {
			vm_region_unmap(region, region->start, start);
			if(region->amap) {
				region->amap_offset += (start - region->start);
			} else {
				region->obj_offset += (start - region->start);
			}
		}
	}

	/* If the start address is changing, we must remove and re-insert the
	 * region in the tree, because the key is changing. */
	if(start != region->start) {
		avl_tree_remove(&region->as->regions, (key_t)region->start);
		avl_tree_insert(&region->as->regions, (key_t)start, region, &region->node);
	}

	/* Modify the addresses in the region. */
	region->start = start;
	region->end = end;
}

/** Split a region.
 * @param region	Region to split.
 * @param end		Where to end first part of region.
 * @param start		Where to start second part of region. */
static void vm_region_split(vm_region_t *region, ptr_t end, ptr_t start) {
	vm_region_t *split;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(end > region->start && end < region->end);
	assert(start >= end && start < region->end);

	/* Create a region structure for the top half. */
	split = vm_region_alloc(region->as, start, region->end, region->flags);

	if(!(region->flags & VM_REGION_RESERVED)) {
		/* Unmap the gap between the regions if there is one. */
		if(end != start) {
			vm_region_unmap(region, end, start);
		}

		/* Copy object details into the split. */
		if((split->handle = region->handle)) {
			object_handle_get(split->handle);
		}
		if((split->amap = region->amap)) {
			refcount_inc(&split->amap->count);
			split->obj_offset = region->obj_offset;
			split->amap_offset = region->amap_offset + (start - region->start);
		} else {
			split->obj_offset = region->obj_offset + (start - region->start);
		}
	}

	/* Change the size of the old region. */
	region->end = end;

	/* Insert the split region. */
	avl_tree_insert(&split->as->regions, (key_t)split->start, split, &split->node);
}

/** Unmap an entire region.
 * @param region	Region to destroy. */
static void vm_region_destroy(vm_region_t *region) {
	/* Unmap the region and drop references to the object/anonymous map. */
	if(!(region->flags & VM_REGION_RESERVED)) {
		vm_region_unmap(region, region->start, region->end);
		if(region->amap) {
			vm_amap_release(region->amap);
		}
		if(region->handle) {
			object_handle_release(region->handle);
		}
	}

	avl_tree_remove(&region->as->regions, region->start);

	/* If the region was the cached find pointer, get rid of it - bad
	 * things will happen if something looks at a freed region. */
	if(region == region->as->find_cache) {
		region->as->find_cache = NULL;
	}
	slab_cache_free(vm_region_cache, region);
}

/** Free a region in an address space.
 * @param as		Address space to free in.
 * @param start		Start of region to free.
 * @param end		End of region to free. */
static void vm_unmap_internal(vm_aspace_t *as, ptr_t start, ptr_t end) {
	vm_region_t *region, *near, *next;

	/* Find the start region. */
	if(!(region = vm_region_find(as, start, &near))) {
		if(near == NULL) {
			/* No region matches, and there is not a region after.
			 * Nothing to do. */
			return;
		} else if(near->start >= end) {
			/* Region following does not overlap the region we're
			 * freeing, do nothing. */
			return;
		}

		/* We need to free some regions following us. */
		region = near;
	} else if(region->start < start) {
		if(region->end == end) {
			/* Just shrink the region and finish. */
			vm_region_shrink(region, region->start, start);
			return;
		} else if(region->end < end) {
			/* Shrink the region, move to next and fall through. */
			vm_region_shrink(region, region->start, start);
			if(!(region = vm_region_next(region))) {
				return;
			}
		} else {
			/* Split the region and finish. */
			vm_region_split(region, start, end);
			return;
		}
	}

	assert(region->start >= start);

	/* Loop through and eat up all the regions necessary. */
	while(region && region->start < end) {
		if(region->end <= end) {
			/* Completely overlap this region, remove. */
			next = vm_region_next(region);
			vm_region_destroy(region);
			region = next;
		} else {
			/* Resize the existing region and finish. */
			vm_region_shrink(region, end, region->end);
			return;
		}
	}
}

/** Searches for free space in an address space.
 * @param as		Address space to search in (should be locked).
 * @param size		Size of space required.
 * @param addrp		Where to store address of space.
 * @return		True if space found, false if not. */
static bool vm_find_free(vm_aspace_t *as, size_t size, ptr_t *addrp) {
	vm_region_t *region, *prev = NULL;

	assert(size);

	/* Iterate over all regions to find the first suitable hole. */
	AVL_TREE_FOREACH(&as->regions, iter) {
		region = avl_tree_entry(iter, vm_region_t);

		if(prev == NULL) {
			/* First region, check if there is a hole preceding it
			 * and whether it is big enough. */
			if((USER_MEMORY_BASE + size) <= region->start) {
				*addrp = USER_MEMORY_BASE;
				return true;
			}
		} else {
			/* Check if there is a gap between the previous region
			 * and this region that's big enough. */
			if((region->start - prev->end) >= size) {
				*addrp = prev->end;
				return true;
			}
		}

		prev = region;
	}

	/* Reached the end of the address space, see if we have space following
	 * the previous entry. If there wasn't a previous entry, the address
	 * space was empty. */
	if(prev) {
		if((prev->end + size) > prev->end && (prev->end + size) <= (USER_MEMORY_BASE + USER_MEMORY_SIZE)) {
			/* We have some space, return it. */
			*addrp = prev->end;
			return true;
		}
	} else if(size <= USER_MEMORY_SIZE) {
		*addrp = USER_MEMORY_BASE;
		return true;
	}

	return false;
}

/** Handle an fault on a region with an anonymous map.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		Whether the fault was successfully handled. */
static bool vm_anon_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	object_handle_t *handle = region->handle;
	vm_amap_t *amap = region->amap;
	phys_ptr_t paddr;
	offset_t offset;
	vm_page_t *page;
	bool write;
	size_t i;
	int ret;

	/* Work out the offset into the object. */
	offset = region->amap_offset + (addr - region->start);
	i = (size_t)(offset >> PAGE_WIDTH);

	mutex_lock(&amap->lock);

	assert(i < amap->max_size);

	/* Do some sanity checks if this is a protection fault. The main fault
	 * handler verifies that the access is allowed by the region flags, so
	 * the only access type protection faults should be is write. COW
	 * faults should never occur on non-private regions, either. */
	if(reason == VM_FAULT_PROTECTION) {
		if(unlikely(access != VM_FAULT_WRITE)) {
			fatal("Non-write protection fault at %p on %p", addr, amap);
		} else if(unlikely(!(region->flags & VM_REGION_PRIVATE))) {
			fatal("Copy-on-write fault at %p on non-private region", addr);
		}
	}

	/* Get the page and work out the flags to map with. */
	write = region->flags & VM_REGION_WRITE;
	if(!amap->pages[i] && !handle) {
		/* No page existing and no source. Allocate a zeroed page. */
		dprintf("vm:  anon fault: no existing page and no source, allocating new\n");
		amap->pages[i] = vm_page_alloc(1, MM_SLEEP | PM_ZERO);
		refcount_inc(&amap->pages[i]->count);
		amap->curr_size++;
		paddr = amap->pages[i]->addr;
	} else if(access == VM_MAP_WRITE) {
		if(amap->pages[i]) {
			assert(refcount_get(&amap->pages[i]->count) > 0);

			/* If the reference count is greater than 1 we must
			 * copy it. Shared regions should not contain any pages
			 * with a reference count greater than 1. */
			if(refcount_get(&amap->pages[i]->count) > 1) {
				assert(region->flags & VM_REGION_PRIVATE);

				dprintf("vm:  anon write fault: copying page %zu due to refcount > 1\n", i);

				page = vm_page_copy(amap->pages[i], MM_SLEEP);

				/* Decrease the count of the old page. We must
				 * handle it going to 0 here, as another object
				 * could have released the page while we were
				 * copying. */
				if(refcount_dec(&amap->pages[i]->count) == 0) {
					vm_page_free(amap->pages[i], 1);
				}

				amap->pages[i] = page;
			}
			
			paddr = amap->pages[i]->addr;
		} else {
			assert(region->flags & VM_REGION_PRIVATE);
			assert(handle);

			/* Find the page to copy. If handling a protection
			 * fault, use the existing mapping address. */
			if(reason == VM_FAULT_PROTECTION) {
				if(unlikely(!page_map_find(&region->as->pmap, addr, &paddr))) {
					fatal("No mapping for %p, but protection fault on it", addr);
				}
			} else {
				assert(handle->object->type->page_get);

				ret = handle->object->type->page_get(handle, offset + region->obj_offset, &page);
				if(unlikely(ret != 0)) {
					dprintf("vm:  could not read page from source (%d)\n", ret);
					mutex_unlock(&amap->lock);
					return false;
				}
				paddr = page->addr;
			}

			dprintf("vm:  anon write fault: copying page 0x%" PRIpp " from %p\n",
			        paddr, handle->object);

			page = vm_page_alloc(1, MM_SLEEP);
			page_copy(page->addr, paddr, MM_SLEEP);

			/* Add the page and release the old one. */
			refcount_inc(&page->count);
			amap->pages[i] = page;
			if(handle->object->type->page_release) {
				handle->object->type->page_release(handle, offset + region->obj_offset, paddr);
			}

			amap->curr_size++;
			paddr = page->addr;
		}
	} else {
		if(amap->pages[i]) {
			assert(refcount_get(&amap->pages[i]->count) > 0);

			/* If the reference count is greater than 1, map read
			 * only. */
			if(refcount_get(&amap->pages[i]->count) > 1) {
				assert(region->flags & VM_REGION_PRIVATE);
				write = false;
			}

			paddr = amap->pages[i]->addr;
		} else {
			assert(region->flags & VM_REGION_PRIVATE);
			assert(handle);
			assert(handle->object->type->page_get);

			/* Get the page from the source, and map read-only. */
			ret = handle->object->type->page_get(handle, offset + region->obj_offset, &page);
			if(unlikely(ret != 0)) {
				dprintf("vm:  could not read page from source (%d)\n", ret);
				mutex_unlock(&amap->lock);
				return false;
			}

			paddr = page->addr;

			dprintf("vm:  anon read fault: mapping page 0x%" PRIpp " from %p as read-only\n",
			        paddr, handle->object);
			write = false;
		}
	}

	/* The page address should now be stored in paddr, and flags should be
	 * set correctly. If this is a protection fault, remove existing
	 * mappings. */
	if(reason == VM_FAULT_PROTECTION) {
		if(unlikely(!page_map_remove(&region->as->pmap, addr, NULL))) {
			fatal("Could not remove previous mapping for %p", addr);
		}

		/* Invalidate the TLB entries. */
		tlb_invalidate(region->as, addr, addr);
	}

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	page_map_insert(&region->as->pmap, addr, paddr, write, region->flags & VM_REGION_EXEC, MM_SLEEP);
	dprintf("vm:  anon fault: mapped 0x%" PRIpp " at %p (as: %p, write: %d)\n",
	        paddr, addr, region->as, write);
	mutex_unlock(&amap->lock);
	return true;
}

/** Handles a fault on objects requiring no special handling.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		Whether the fault was successfully handled. */
static bool vm_generic_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	phys_ptr_t paddr;
	bool write, exec;
	vm_page_t *page;
	offset_t offset;
	int ret;

	assert(region->handle);
	assert(region->handle->object->type->page_get);

	/* Get a page from the object. */
	offset = (offset_t)(addr - region->start) + region->obj_offset;
	ret = region->handle->object->type->page_get(region->handle, offset, &page);
	if(unlikely(ret != 0)) {
		dprintf("vm:  failed to get page for %p (%d)\n", addr, ret);
		return false;
	}

	/* Check if a mapping already exists. This is possible if two threads
	 * in a process on different CPUs fault on the same address
	 * simultaneously. */
	if(page_map_find(&region->as->pmap, addr, &paddr)) {
		if(paddr != page->addr) {
			fatal("Incorrect existing mapping found (found %" PRIpp", should be %" PRIpp ")",
			      paddr, page->addr);
		} else if(region->handle->object->type->page_release) {
			region->handle->object->type->page_release(region->handle, offset, page->addr);
		}
		return true;
	}

	/* Work out the flags to map with. */
	write = region->flags & VM_REGION_WRITE;
	exec = region->flags & VM_REGION_EXEC;

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	page_map_insert(&region->as->pmap, addr, page->addr, write, exec, MM_SLEEP);
	dprintf("vm:  mapped 0x%" PRIpp " at %p (as: %p, write: %d, exec: %d)\n",
	        page->addr, addr, region->as, write, exec);
	return true;
}

/** Page fault handler.
 * @param addr		Address the fault occurred at.
 * @param reason	Reason for the fault.
 * @param access	Type of memory access that caused the fault.
 * @return		Whether the fault was handled. */
bool vm_fault(ptr_t addr, int reason, int access) {
	vm_aspace_t *as = curr_aspace;
	vm_region_t *region;
	bool ret = false;

	/* If we don't have an address space, don't do anything. */
	if(!as) {
		return false;
	}

	dprintf("vm: page fault at %p (as: %p, reason: %d, access: %d)\n", addr, as, reason, access);

	/* Round down address to a page boundary. */
	addr &= PAGE_MASK;

	/* Safe to take the lock despite us being in an interrupt - the lock
	 * is only held within the functions in this file, and they should not
	 * incur a pagefault (if they do there's something wrong!). */
	mutex_lock(&as->lock);

	/* Find the region that the fault occurred in - if its a reserved
	 * region, the memory is unmapped so treat it as though no region is
	 * there. */
	if(unlikely(!(region = vm_region_find(as, addr, NULL)))) {
		goto out;
	} else if(unlikely(region->flags & VM_REGION_RESERVED)) {
		goto out;
	}

	assert(region->amap || region->handle);

	/* Check whether the access is allowed. Fault codes are defined to the
	 * same value as region protection flags. */
	if(!(region->flags & access)) {
		goto out;
	}

	/* If the region is a stack region, check if we've hit the guard page.
	 * TODO: Stack direction. */
	if(region->flags & VM_REGION_STACK && addr == region->start) {
		kprintf(LOG_DEBUG, "vm: thread %" PRIu32 " hit stack guard page %p\n",
		        curr_thread->id, addr);
		return false;
	}

	/* Call the anonymous fault handler if there is an anonymous map, or
	 * pass the fault through to the object if it has its own fault
	 * handler. */
	if(region->amap) {
		ret = vm_anon_fault(region, addr, reason, access);
	} else if(region->handle->object->type->fault) {
		ret = region->handle->object->type->fault(region, addr, reason, access);
	} else {
		ret = vm_generic_fault(region, addr, reason, access);
	}
out:
	mutex_unlock(&as->lock);
	return ret;
}

/** Mark a region as reserved.
 *
 * Marks a region of memory in an address space as reserved. Reserved regions
 * will never be allocated from if mapping without VM_MAP_FIXED, but they can
 * be overwritten with VM_MAP_FIXED mappings or removed by using vm_unmap() on
 * the region.
 *
 * @param as		Address space to reserve in.
 * @param start		Start of region to reserve.
 * @param size		Size of region to reserve.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_reserve(vm_aspace_t *as, ptr_t start, size_t size) {
	vm_region_t *region;

	if(!size || start % PAGE_SIZE || size % PAGE_SIZE || !vm_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&as->lock);

	/* Allocate the region structure. */
	region = vm_region_alloc(as, start, start + size, VM_REGION_RESERVED);

	/* Create a hole and insert it into the address space. */
	vm_unmap_internal(as, start, start + size);
	avl_tree_insert(&as->regions, (key_t)region->start, region, &region->node);

	dprintf("vm: reserved region [%p,%p) (as: %p)\n",region->start, region->end, as);
	mutex_unlock(&as->lock);
	return 0;
}

/** Map an object into memory.
 *
 * Creates a new memory mapping within an address space that maps either an
 * object or anonymous memory. If the VM_MAP_FIXED flag is specified, then the
 * region will be mapped at the exact location specified, and any existing
 * mappings in the same region will be overwritten. Otherwise, a region of
 * unused space will be allocated for the mapping. If the VM_MAP_PRIVATE flag
 * is specified, modifications to the mapping will not be transferred through
 * to the source object, and if the address space is duplicated, the duplicate
 * and original will be given copy-on-write copies of the region. If the
 * VM_MAP_PRIVATE flag is not specified and the address space is duplicated,
 * changes made in either address space will be visible in the other.
 *
 * @param as		Address space to map in.
 * @param start		Start address of region (if VM_MAP_FIXED). Must be a
 *			multiple of the system page size.
 * @param size		Size of region to map. Must be a multiple of the system
 *			page size.
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param handle	Handle to object to map in. If NULL, then the region
 *			will be an anonymous memory mapping.
 * @param offset	Offset into object to map from.
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags, object_handle_t *handle,
           offset_t offset, ptr_t *addrp) {
	vm_region_t *region;
	int rflags, ret;

	/* Check whether the supplied arguments are valid. */
	if(!size || size % PAGE_SIZE || offset % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	}
	if(flags & VM_MAP_FIXED) {
		if(start % PAGE_SIZE || !vm_region_fits(start, size)) {
			return -ERR_PARAM_INVAL;
		}
	} else if(!addrp) {
		return -ERR_PARAM_INVAL;
	}
	if(handle) {
		/* Cannot create private mappings to objects requiring special
		 * fault handling. */
		if(flags & VM_MAP_PRIVATE && handle->object->type->fault) {
			return -ERR_NOT_SUPPORTED;
		}

		/* Check if the object can be mapped in with the given flags. */
		if(handle->object->type->mappable) {
			assert(handle->object->type->page_get || handle->object->type->fault);
			if((ret = handle->object->type->mappable(handle, flags)) != 0) {
				return ret;
			}
		} else {
			if(!handle->object->type->page_get && !handle->object->type->fault) {
				return -ERR_NOT_SUPPORTED;
			}
		}
	}

	/* Convert mapping flags to region flags. The flags with a region
	 * equivalent have the same value. */
	rflags = flags & (VM_MAP_READ | VM_MAP_WRITE | VM_MAP_EXEC | VM_MAP_PRIVATE | VM_MAP_STACK);

	mutex_lock(&as->lock);

	/* If the mapping is fixed, remove anything in the location we want to
	 * insert into, otherwise find some free space. */
	if(flags & VM_MAP_FIXED) {
		vm_unmap_internal(as, start, start + size);
	} else {
		if(!vm_find_free(as, size, &start)) {
			mutex_unlock(&as->lock);
			return -ERR_NO_MEMORY;
		}
	}

	/* Create the region structure, attach the object to it, and create an
	 * anonymous map if necessary. */
	region = vm_region_alloc(as, start, start + size, rflags);
	if((region->handle = handle)) {
		object_handle_get(region->handle);
		region->obj_offset = offset;
	}
	if(!handle || (flags & VM_MAP_PRIVATE)) {
		region->amap = vm_amap_create(size);

		/* Should not fail to reference since it is newly created. */
		if(vm_amap_map(region->amap, 0, size) != 0) {
			fatal("Could not reference new anonymous map");
		}
	}

	/* Insert the region into the tree. */
	avl_tree_insert(&as->regions, (key_t)region->start, region, &region->node);

	dprintf("vm: mapped region [%p,%p) (as: %p, handle: %p, flags(m/r): %d/%d)\n",
	        region->start, region->end, as, handle, flags, rflags);
	if(addrp) {
		*addrp = region->start;
	}
	mutex_unlock(&as->lock);
	return 0;
}

/** Unmaps a region of memory.
 *
 * Marks the specified address range as free in an address space and unmaps
 * anything that may be mapped there.
 *
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_unmap(vm_aspace_t *as, ptr_t start, size_t size) {
	if(!size || start % PAGE_SIZE || size % PAGE_SIZE || !vm_region_fits(start, size)) {
		return -ERR_PARAM_INVAL;
	}

	mutex_lock(&as->lock);
	vm_unmap_internal(as, start, start + size);
	mutex_unlock(&as->lock);

	dprintf("vm: unmapped region [%p,%p) (as: %p)\n", start, start + size, as);
	return 0;
}

/** Switch to another address space.
 * @note		Does not take address space lock because this function
 *			is used during rescheduling.
 * @param as		Address space to switch to (if NULL, then will switch
 *			to the kernel address space). */
void vm_aspace_switch(vm_aspace_t *as) {
	bool state = intr_disable();

	/* Decrease old address space's reference count, if there is one. */
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
 * @return		Pointer to address space structure. */
vm_aspace_t *vm_aspace_create(void) {
	vm_aspace_t *as;
	int ret;

	as = slab_cache_alloc(vm_aspace_cache, MM_SLEEP);
	page_map_init(&as->pmap, MM_SLEEP);
	as->find_cache = NULL;

	/* Mark the first page of the address space as reserved to catch NULL
	 * pointer accesses. This should not fail. */
	ret = vm_reserve(as, 0x0, PAGE_SIZE);
	assert(ret == 0);
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
void vm_aspace_destroy(vm_aspace_t *as) {
	assert(as);

	if(refcount_get(&as->count) > 0) {
		fatal("Destroying in-use address space");
	}

	/* Unmap and destroy each region. */
	AVL_TREE_FOREACH_SAFE(&as->regions, iter) {
		vm_region_destroy(avl_tree_entry(iter, vm_region_t));
	}

	/* Destroy the page map. */
	page_map_destroy(&as->pmap);

	slab_cache_free(vm_aspace_cache, as);
}

/** Initialise the address space caches. */
void __init_text vm_init(void) {
	vm_aspace_cache = slab_cache_create("vm_aspace_cache", sizeof(vm_aspace_t),
	                                    0, vm_aspace_ctor, NULL, NULL, NULL,
	                                    SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
	vm_region_cache = slab_cache_create("vm_region_cache", sizeof(vm_region_t),
	                                    0, NULL, NULL, NULL, NULL, SLAB_DEFAULT_PRIORITY,
	                                    NULL, 0, MM_FATAL);
	vm_amap_cache = slab_cache_create("vm_amap_cache", sizeof(vm_amap_t),
	                                  0, vm_amap_ctor, NULL, NULL,
	                                  NULL, SLAB_DEFAULT_PRIORITY, NULL, 0, MM_FATAL);
}

/** Dump an address space.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_aspace(int argc, char **argv) {
	vm_region_t *region;
	process_t *process;
	vm_aspace_t *as;
	unative_t val;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [--addr] <value>\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints the contents of an address space. If the --addr option is specified, the\n");
		kprintf(LOG_NONE, "value will be taken as an address of an address space structure. Otherwise it\n");
		kprintf(LOG_NONE, "is taken as a process ID, and that process' address space is printed.\n");
		return KDBG_OK;
	} else if(argc < 2 || argc > 3) {
		kprintf(LOG_NONE, "Expression expected. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(argc == 3) {
		if(strcmp(argv[1], "--addr") != 0) {
			kprintf(LOG_NONE, "Unknown option '%s'\n", argv[1]);
			return KDBG_FAIL;
		} else if(kdbg_parse_expression(argv[2], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		}

		as = (vm_aspace_t *)((ptr_t)val);
	} else {
		if(kdbg_parse_expression(argv[1], &val, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(!(process = process_lookup_unsafe(val))) {
			kprintf(LOG_NONE, "Invalid process ID.\n");
			return KDBG_FAIL;
		}

		as = process->aspace;
	}

	kprintf(LOG_NONE, "%-18s %-18s %-5s %-18s %-10s %-18s %s\n",
	        "Base", "End", "Flags", "Handle", "Offset", "Amap", "Offset");
	kprintf(LOG_NONE, "%-18s %-18s %-5s %-18s %-10s %-18s %s\n",
	        "====", "===", "=====", "======", "======", "====", "======");

	AVL_TREE_FOREACH(&as->regions, iter) {
		region = avl_tree_entry(iter, vm_region_t);

		kprintf(LOG_NONE, "%-18p %-18p %-5d %-18p %-10" PRId64 " %-18p %" PRId64 "\n",
		        region->start, region->end, region->flags,
		        region->handle, region->obj_offset, region->amap,
		        region->amap_offset);
	}

	return KDBG_OK;
}

/** Map an object into memory.
 *
 * Creates a new memory mapping within an address space that maps either an
 * object or anonymous memory. If the VM_MAP_FIXED flag is specified, then the
 * region will be mapped at the exact location specified, and any existing
 * mappings in the same region will be overwritten. Otherwise, a region of
 * unused space will be allocated for the mapping. If the VM_MAP_PRIVATE flag
 * is specified, modifications to the mapping will not be transferred through
 * to the source object, and if the address space is duplicated, the duplicate
 * and original will be given copy-on-write copies of the region. If the
 * VM_MAP_PRIVATE flag is not specified and the address space is duplicated,
 * changes made in either address space will be visible in the other.
 *
 * @param args		Pointer to arguments structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_vm_map(vm_map_args_t *args) {
	object_handle_t *obj = NULL;
	vm_map_args_t kargs;
	int ret, err;
	ptr_t addr;

	if((ret = memcpy_from_user(&kargs, args, sizeof(vm_map_args_t))) != 0) {
		return ret;
	} else if(!(kargs.flags & VM_MAP_FIXED) && !kargs.addrp) {
		return -ERR_PARAM_INVAL;
	} else if(kargs.handle >= 0) {
		if((ret = object_handle_lookup(curr_proc, kargs.handle, -1, &obj)) != 0) {
			return ret;
		}
	}

	ret = vm_map(curr_proc->aspace, (ptr_t)kargs.start, kargs.size, kargs.flags,
	             obj, kargs.offset, &addr);
	if(ret == 0 && kargs.addrp) {
		if((err = memcpy_to_user(kargs.addrp, &addr, sizeof(void *))) != 0) {
			vm_unmap(curr_proc->aspace, addr, kargs.size);
			ret = err;
		}
	}

	if(obj) {
		object_handle_release(obj);
	}
	return ret;
}

/** Unmaps a region of memory.
 *
 * Marks the specified address range as free in the calling process' address
 * space and unmaps anything that may be mapped there.
 *
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_vm_unmap(void *start, size_t size) {
	return vm_unmap(curr_proc->aspace, (ptr_t)start, size);
}
