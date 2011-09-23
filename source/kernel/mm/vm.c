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
 * Some details on the various region trees/lists and the method used for
 * allocation of free regions.
 *  - There is a vm_region_t structure for each region of memory within an
 *    address space, whether allocated or not.
 *  - There is an AVL tree containing only allocated regions, used for fast
 *    region lookups upon page faults. We do not care about free regions when
 *    doing these lookups, as a page fault on a free region is invalid, so
 *    including free regions in this tree would be an unnecessary bottleneck.
 *  - Free regions are attached to power of two free lists to allow fast
 *    allocation of free space for non-fixed mappings.
 *  - There is a sorted list of all regions in an address space. This is used
 *    on unmap operations to be able to find all the regions that the unmap
 *    covers.
 *
 * A brief note about reference counting for pages in the anonymous memory
 * layer:
 *  - The reference count in the page structure is used to track how many
 *    anonymous objects refer to a single page (i.e. object has been duplicated
 *    but the page has not been copied, because no write fault has occurred).
 *    If, when a write fault occurs on a page, the page structure reference
 *    count is greater than 1, the page is copied. Otherwise, the page is just
 *    remapped as read-write (if the region is VM_REGION_WRITE, that is).
 *  - Each object also contains an array of reference counts for each page that
 *    the object can cover. This array is used to track how many regions are
 *    mapping each page of the object, allowing pages to be freed when no more
 *    regions refer to them.
 *
 *
 * @todo		The anonymous object page array could be changed into a
 *			two-level array, which would reduce memory consumption
 *			for large, sparsely-used objects.
 * @todo		Swap support.
 */

#include <arch/memory.h>

#include <cpu/intr.h>
#include <cpu/ipi.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/phys.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <kdbg.h>
#include <status.h>

#include "vm_priv.h"

/** VM-managed portion of the kernel address space. */
vm_aspace_t *kernel_aspace = NULL;

/** Slab caches used for VM structures. */
static slab_cache_t *vm_aspace_cache;
static slab_cache_t *vm_region_cache;
static slab_cache_t *vm_amap_cache;

/** Constructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void vm_aspace_ctor(void *obj, void *data) {
	vm_aspace_t *as = obj;
	int i;

	mutex_init(&as->lock, "vm_aspace_lock", 0);
	refcount_set(&as->count, 0);
	avl_tree_init(&as->tree);
	list_init(&as->regions);

	for(i = 0; i < VM_FREELISTS; i++) {
		list_init(&as->free[i]);
	}
}

/** Constructor for anonymous map objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void vm_amap_ctor(void *obj, void *data) {
	vm_amap_t *map = obj;

	mutex_init(&map->lock, "vm_amap_lock", 0);
}

/** Check if a region fits in an address space.
 * @param as		Address space to check in.
 * @param start		Start address of region.
 * @param size		Size of region.
 * @return		Whether region fits. */
static inline bool vm_region_fits(vm_aspace_t *as, ptr_t start, size_t size) {
	vm_region_t *first, *last;
	ptr_t end;

	assert(!list_empty(&as->regions));

	/* Get the first and last regions in the address space. */
	first = list_entry(as->regions.next, vm_region_t, header);
	last = list_entry(as->regions.prev, vm_region_t, header);
	end = start + size;

	return (end >= start && start >= first->start && end <= last->end);
}

/** Check if a region is in use.
 * @param region	Region to check.
 * @return		Whether the region is in use. Region is classed as
 *			in use if it is not free and not reserved. */
static inline bool vm_region_used(const vm_region_t *region) {
	return (region->flags && !(region->flags & VM_REGION_RESERVED));
}

/** Check if two regions can be merged.
 * @param a		First region.
 * @param b		Second region.
 * @return		Whether the regions can be merged. Regions are only
 *			mergeable if unused. */
static inline bool vm_region_mergeable(const vm_region_t *a, const vm_region_t *b) {
	if(!vm_region_used(a) && !vm_region_used(b)) {
		if((a->flags & VM_REGION_RESERVED) == (b->flags & VM_REGION_RESERVED)) {
			return true;
		}
	}
	return false;
}

/** Add a region to the appropriate free list.
 * @param region	Region to add.
 * @param size		Size of the region. */
static inline void vm_freelist_insert(vm_region_t *region, size_t size) {
	int list = highbit(size) - PAGE_WIDTH - 1;

	assert(!region->flags);
	list_append(&region->as->free[list], &region->free_link);
	region->as->free_map |= ((ptr_t)1 << list);
}

/** Remove a region from its free list.
 * @param region	Region to remove. */
static inline void vm_freelist_remove(vm_region_t *region) {
	int list = highbit(region->end - region->start) - PAGE_WIDTH - 1;

	assert(!region->flags);
	list_remove(&region->free_link);
	if(list_empty(&region->as->free[list])) {
		region->as->free_map &= ~((ptr_t)1 << list);
	}
}

/** Check if a freelist is empty.
 * @param as		Address space to check in.
 * @param list		List number.
 * @return		Whether the list is empty. */
static inline bool vm_freelist_empty(vm_aspace_t *as, int list) {
	if(as->free_map & ((ptr_t)1 << list)) {
		assert(!list_empty(&as->free[list]));
		return false;
	} else {
		assert(list_empty(&as->free[list]));
		return true;
	}
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
	map->pages = kcalloc(map->max_size, sizeof(page_t *), MM_SLEEP);
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
 * @return		Status code describing result of the operation. */
static status_t vm_amap_map(vm_amap_t *map, offset_t offset, size_t size) {
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
			return STATUS_NO_MEMORY;
		}
		map->rref[i]++;
	}

	mutex_unlock(&map->lock);
	return STATUS_SUCCESS;
}

/** Allocate a new region structure. Caller must attach object to it.
 * @param as		Address space of the region.
 * @param start		Start address of the region.
 * @param end		End address of the region.
 * @param flags		Flags for the region.
 * @return		Pointer to region structure. */
static vm_region_t *vm_region_create(vm_aspace_t *as, ptr_t start, ptr_t end, int flags) {
	vm_region_t *region;

	region = slab_cache_alloc(vm_region_cache, MM_SLEEP);
	list_init(&region->header);
	list_init(&region->free_link);
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

/** Clone a region.
 * @param src		Region to clone.
 * @param as		Address space for new region.
 * @return		Pointer to cloned region. */
static vm_region_t *vm_region_clone(vm_region_t *src, vm_aspace_t *as) {
	size_t i, start, end;
	vm_region_t *dest;

	dest = vm_region_create(as, src->start, src->end, src->flags);
	if(!vm_region_used(src)) {
		return dest;
	}

	/* Copy the object handle. */
	if(src->handle) {
		object_handle_get(src->handle);
		dest->handle = src->handle;
		dest->obj_offset = src->obj_offset;
	}

	/* If this is not a private mapping, just point the new region at the
	 * old anonymous map and return. */
	if(!(src->flags & VM_REGION_PRIVATE)) {
		if(src->amap) {
			refcount_inc(&src->amap->count);
			vm_amap_map(src->amap, src->amap_offset, src->end - src->start);
			dest->amap = src->amap;
			dest->amap_offset = src->amap_offset;
		}
		return dest;
	}

	/* This is a private region. We must create a copy of the area within
	 * the source anonymous map that the region points to, so work out the
	 * entries within the map that the region covers. */
	assert(src->amap);
	mutex_lock(&src->amap->lock);
	start = (size_t)(src->amap_offset >> PAGE_WIDTH);
	end = start + (size_t)((src->end - src->start) >> PAGE_WIDTH);
	assert(end <= src->amap->max_size);

	/* Create a new map. */
	dest->amap = slab_cache_alloc(vm_amap_cache, MM_SLEEP);
	refcount_set(&dest->amap->count, 1);
	dest->amap->curr_size = 0;
	dest->amap->max_size = end - start;
	dest->amap->pages = kcalloc(dest->amap->max_size, sizeof(page_t *), MM_SLEEP);
	dest->amap->rref = kcalloc(dest->amap->max_size, sizeof(uint16_t *), MM_SLEEP);

	/* Write-protect all mappings on the source region. */
	mmu_context_lock(src->as->mmu);
	for(i = src->start; i < src->end; i += PAGE_SIZE) {
		mmu_context_protect(src->as->mmu, i, false, src->flags & VM_REGION_EXEC);
	}
	mmu_context_unlock(src->as->mmu);

	/* Point all of the pages in the new map to the pages from the source
	 * map: they will be copied when a write fault occurs on either the
	 * source or the destination. Set the region reference count for each
	 * page to 1, to account for the destination region. */
	for(i = start; i < end; i++) {
		if(src->amap->pages[i]) {
			refcount_inc(&src->amap->pages[i]->count);
			dest->amap->curr_size++;
		}
		dest->amap->pages[i - start] = src->amap->pages[i];
		dest->amap->rref[i - start] = 1;
	}

	dprintf("vm: copied private region %p (map: %p) to %p (map: %p)\n",
	        src, src->amap, dest, dest->amap);
	mutex_unlock(&src->amap->lock);
	return dest;
}

/** Get the region before another region in the region list.
 * @param region	Region to get region before from.
 * @return		Pointer to previous region, or NULL if start of list. */
static vm_region_t *vm_region_prev(vm_region_t *region) {
	if(region->as->regions.next == &region->header) {
		return NULL;
	}

	return list_entry(region->header.prev, vm_region_t, header);
}

/** Get the region after another region in the region list.
 * @param region	Region to get region after from.
 * @return		Pointer to next region, or NULL if end of list. */
static vm_region_t *vm_region_next(vm_region_t *region) {
	if(region->as->regions.prev == &region->header) {
		return NULL;
	}

	return list_entry(region->header.next, vm_region_t, header);
}

/** Searches for a region containing an address.
 * @param as		Address space to search in (should be locked).
 * @param addr		Address to search for.
 * @param unused	Whether to include unused regions in the search.
 * @return		Pointer to region if found, false if not. If including
 *			unused regions, this will always succeed unless the
 *			given address is invalid. */
static vm_region_t *vm_region_find(vm_aspace_t *as, ptr_t addr, bool unused) {
	avl_tree_node_t *node, *near = NULL;
	vm_region_t *region;

	/* Check if the cached pointer matches. Caching the last found region
	 * helps mainly for page fault handling when code is hitting different
	 * parts of a newly-mapped region in succession. */
	if(as->find_cache && as->find_cache->start <= addr && as->find_cache->end > addr) {
		return as->find_cache;
	}

	/* Fall back on searching through the AVL tree. */
	node = as->tree.root;
	while(node) {
		region = avl_tree_entry(node, vm_region_t);
		assert(vm_region_used(region));
		if(addr >= region->start) {
			if(addr < region->end) {
				as->find_cache = region;
				return region;
			}

			near = node;
			node = node->right;
		} else {
			node = node->left;
		}
	}

	/* We couldn't find a matching allocated region. We do however have the
	 * nearest allocated region on the left. If we want to include unused
	 * regions in the search, search forward from this region in the region
	 * list to find the required region. */
	if(unused) {
		if(near) {
			region = vm_region_next(avl_tree_entry(near, vm_region_t));
		} else {
			/* Should never be empty. */
			assert(!list_empty(&as->regions));
			region = list_entry(as->regions.next, vm_region_t, header);
		}

		while(region) {
			if(addr >= region->start && addr < region->end) {
				assert(!vm_region_used(region));
				return region;
			}

			region = vm_region_next(region);
		}
	}

	return NULL;
}

/** Release a page that was mapped in a region.
 * @param region	Region that the page was mapped in.
 * @param offset	Offset into the region the page was mapped at.
 * @param phys		Physical address that was unmapped. */
static void vm_region_release_page(vm_region_t *region, offset_t offset, phys_ptr_t phys) {
	size_t i;

	if(region->amap) {
		offset += region->amap_offset;
		i = (size_t)(offset >> PAGE_WIDTH);

		assert(i < region->amap->max_size);

		/* If page is in the object, then do nothing. */
		if(region->amap->pages[i]) {
			assert(region->amap->pages[i]->addr == phys);
			return;
		}

		/* Page must have come from source, release it there. */
		assert(region->handle);
		assert(region->handle->object->type->release_page);

		offset += region->obj_offset;
		region->handle->object->type->release_page(region->handle, offset, phys);
	} else if(region->handle->object->type->release_page) {
		offset += region->obj_offset;
		region->handle->object->type->release_page(region->handle, offset, phys);
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
	phys_ptr_t phys;
	offset_t offset;
	ptr_t addr;
	size_t i;

	assert(vm_region_used(region));
	assert(region->handle || region->amap);

	/* Acquire the anonymous map lock if there is one. */
	if(region->amap) {
		mutex_lock(&region->amap->lock);
	}

	mmu_context_lock(region->as->mmu);

	for(addr = start; addr < end; addr += PAGE_SIZE) {
		offset = (offset_t)(addr - region->start);

		/* Unmap the page and release it from its source. */
		if(mmu_context_unmap(region->as->mmu, addr, true, &phys)) {
			vm_region_release_page(region, offset, phys);
		}

		/* Update the region reference count on the anonymous map. */
		if(region->amap) {
			offset += region->amap_offset;
			i = (size_t)(offset >> PAGE_WIDTH);

			assert(i < region->amap->max_size);
			assert(region->amap->rref[i]);

			if(--region->amap->rref[i] == 0 && region->amap->pages[i]) {
				dprintf("vm: anon object rref %zu reached 0, freeing 0x%" PRIxPHYS " (amap: %p)\n",
				        i, region->amap->pages[i]->addr, region->amap);
				if(refcount_dec(&region->amap->pages[i]->count) == 0) {
					page_free(region->amap->pages[i]);
				}
				region->amap->pages[i] = NULL;
				region->amap->curr_size--;
			}
		}
	}

	mmu_context_unlock(region->as->mmu);

	if(region->amap) {
		mutex_unlock(&region->amap->lock);
	}
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

	/* Unmap pages in the areas we're not going to cover any more. */
	if(vm_region_used(region)) {
		if(end < region->end) {
			vm_region_unmap(region, end, region->end);
		}
		if(start > region->start) {
			vm_region_unmap(region, region->start, start);
			if(region->amap) {
				region->amap_offset += (start - region->start);
			} else {
				region->obj_offset += (start - region->start);
			}
		}

		/* If the start address is changing, we must re-insert the
		 * region in the tree, because the key is changing. */
		if(start != region->start) {
			avl_tree_remove(&region->as->tree, &region->tree_link);
			avl_tree_insert(&region->as->tree, &region->tree_link, start, region);
		}
	} else if(!(region->flags & VM_REGION_RESERVED)) {
		/* If the size is changing, remove from the current freelist. */
		if((region->end - region->start) != (end - start)) {
			vm_freelist_remove(region);
			vm_freelist_insert(region, end - start);
		}
	}

	/* Modify the addresses in the region. */
	region->start = start;
	region->end = end;
}

/** Split a region.
 * @param region	Region to split. Upon return this structure will be the
 *			lower half of the split.
 * @param end		Where to end first part of region.
 * @param start		Where to start second part of region. */
static void vm_region_split(vm_region_t *region, ptr_t end, ptr_t start) {
	vm_region_t *split;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));
	assert(end > region->start && end < region->end);
	assert(start >= end && start < region->end);

	/* Create a region structure for the top half. */
	split = vm_region_create(region->as, start, region->end, region->flags);

	if(vm_region_used(region)) {
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

		/* Insert the split region. */
		avl_tree_insert(&split->as->tree, &split->tree_link, split->start, split);
	} else if(!(region->flags & VM_REGION_RESERVED)) {
		/* Move the bottom half to the correct free list. */
		vm_freelist_remove(region);
		vm_freelist_insert(region, end - region->start);

		/* Add the split region to a free list. */
		vm_freelist_insert(split, split->end - start);
	}

	list_add_after(&region->header, &split->header);

	/* Change the size of the old region. */
	region->end = end;
}

/** Unmap an entire region.
 * @param region	Region to destroy. */
static void vm_region_destroy(vm_region_t *region) {
	/* Unmap the region and drop references to the object/anonymous map,
	 * and remove it from the tree or freelist. */
	if(vm_region_used(region)) {
		vm_region_unmap(region, region->start, region->end);
		if(region->amap) {
			vm_amap_release(region->amap);
		}
		if(region->handle) {
			object_handle_release(region->handle);
		}

		avl_tree_remove(&region->as->tree, &region->tree_link);
	} else if(!(region->flags & VM_REGION_RESERVED)) {
		vm_freelist_remove(region);
	}

	/* Remove from the main region list. */
	list_remove(&region->header);

	/* If the region was the cached find pointer, get rid of it. */
	if(region == region->as->find_cache) {
		region->as->find_cache = NULL;
	}

	assert(list_empty(&region->free_link));
	slab_cache_free(vm_region_cache, region);
}

/** Remove all existing regions in an address range.
 * @param as		Address space to free in.
 * @param start		Start of region to free.
 * @param end		End of region to free.
 * @return		Pointer to region before the hole created. */
static vm_region_t *vm_region_insert_internal(vm_aspace_t *as, ptr_t start, ptr_t end) {
	vm_region_t *region, *next, *prev;

	/* Find the region containing the start address. */
	region = vm_region_find(as, start, true);
	assert(region);

	if(region->start < start) {
		prev = region;
		if(region->end <= end) {
			vm_region_shrink(region, region->start, start);

			region = vm_region_next(region);
			if(prev->end == end || !region) {
				return prev;
			}
		} else {
			/* Split the region and finish. */
			vm_region_split(region, start, end);
			return region;
		}
	} else {
		/* Save the region before this one to return to the caller. */
		prev = vm_region_prev(region);
	}

	/* Loop through and eat up all the regions necessary. */
	while(true) {
		if(region->end <= end) {
			/* Completely overlap this region, remove. */
			next = (region->end < end) ? vm_region_next(region) : NULL;
			vm_region_destroy(region);
			if(!(region = next)) {
				break;
			}
		} else {
			/* Resize the existing region and finish. */
			vm_region_shrink(region, end, region->end);
			break;
		}
	}

	return prev;
}

/** Insert a region, replacing overlapping existing regions.
 * @note		Addresses must be checked for validity by the caller.
 * @param as		Address space to insert into.
 * @param start		Start of region to insert.
 * @param end		End of region to insert.
 * @param flags		Flags for the new region.
 * @return		Pointer to inserted region. May not start or end at the
 *			requested addresses if inserting an unused region. This
 *			region will have been inserted into the tree or free
 *			lists as necessary. */
static vm_region_t *vm_region_insert(vm_aspace_t *as, ptr_t start, ptr_t end, int flags) {
	vm_region_t *region, *exist;

	/* Create the new region. */
	region = vm_region_create(as, start, end, flags);

	/* Create a hole to insert the new region into. */
	exist = vm_region_insert_internal(as, start, end);
	if(exist) {
		assert(exist->end == start);

		list_add_after(&exist->header, &region->header);

		/* If the existing region and this region are compatible,
		 * merge them. */
		if(vm_region_mergeable(region, exist)) {
			region->start = exist->start;
			vm_region_destroy(exist);
		}
	} else {
		list_prepend(&as->regions, &region->header);
	}

	/* Check if we can merge with the region after. */
	exist = vm_region_next(region);
	if(exist) {
		assert(exist->start == end);

		if(vm_region_mergeable(region, exist)) {
			region->end = exist->end;
			vm_region_destroy(exist);
		}
	}

	/* Finally, insert into the region tree or the free lists. */
	if(vm_region_used(region)) {
		avl_tree_insert(&as->tree, &region->tree_link, region->start, region);
	} else if(!(region->flags & VM_REGION_RESERVED)) {
		vm_freelist_insert(region, region->end - region->start);
	}

	return region;
}

/** Allocate space in an address space.
 * @param as		Address space to allocate in (should be locked).
 * @param size		Size of space required.
 * @param flags		Flags for the region. Should not be empty or reserved.
 * @return		Pointer to region if allocated, NULL if not. */
static vm_region_t *vm_region_alloc(vm_aspace_t *as, size_t size, int flags) {
	vm_region_t *region;
	size_t exist_size;
	int list, i;

	assert(size);
	assert(flags && !(flags & VM_REGION_RESERVED));

	/* Get the list to search on. If the size is exactly a power of 2, then
	 * regions on freelist[n] are guaranteed to be big enough. Otherwise,
	 * use freelist[n + 1] so that we ensure that all regions we find are
	 * large enough. The free bitmap check will ensure that list does not
	 * go higher than the number of freelists. */
	list = highbit(size) - PAGE_WIDTH - 1;
	if((size & (size - 1)) != 0 && as->free_map >> (list + 1)) {
		list++;
	}

	/* Find a free region. */
	for(i = list; i < VM_FREELISTS; i++) {
		if(vm_freelist_empty(as, i)) {
			continue;
		}

		LIST_FOREACH(&as->free[i], iter) {
			region = list_entry(iter, vm_region_t, free_link);
			assert(!vm_region_used(region));

			exist_size = region->end - region->start;
			if(exist_size < size) {
				continue;
			} else if(exist_size > size) {
				/* Too big, split it. */
				vm_region_split(region, region->start + size, region->start + size);
			}

			/* Remove from the free list and add to the tree. */
			vm_freelist_remove(region);
			avl_tree_insert(&as->tree, &region->tree_link, region->start, region);
			region->flags = flags;
			dprintf("vm: allocated region [%p,%p) from list %d (as: %p)\n",
			        region->start, region->end, i, as);
			return region;
		}
	}

	return NULL;
}

/** Handle an fault on a region with an anonymous map.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		VM_FAULT_* code describing result of the fault. */
static int vm_anon_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	object_handle_t *handle = region->handle;
	vm_amap_t *amap = region->amap;
	phys_ptr_t paddr;
	offset_t offset;
	page_t *page;
	status_t ret;
	bool write;
	size_t i;

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
			fatal("Non-write protection fault at %p on %p (%d)", addr, amap, access);
		} else if(unlikely(!(region->flags & VM_REGION_PRIVATE))) {
			fatal("Copy-on-write fault at %p on non-private region", addr);
		}
	}

	/* Get the page and work out the flags to map with. */
	write = region->flags & VM_REGION_WRITE;
	if(!amap->pages[i] && !handle) {
		/* No page existing and no source. Allocate a zeroed page. */
		dprintf("vm:  anon fault: no existing page and no source, allocating new\n");
		amap->pages[i] = page_alloc(MM_SLEEP | PM_ZERO);
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

				page = page_copy(amap->pages[i], MM_SLEEP);
				refcount_inc(&page->count);

				/* Decrease the count of the old page. We must
				 * handle it going to 0 here, as another object
				 * could have released the page while we were
				 * copying. */
				if(refcount_dec(&amap->pages[i]->count) == 0) {
					page_free(amap->pages[i]);
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
				if(unlikely(!mmu_context_query(region->as->mmu, addr, &paddr, NULL, NULL))) {
					fatal("No mapping for %p, but protection fault on it", addr);
				}
			} else {
				assert(handle->object->type->get_page);

				ret = handle->object->type->get_page(handle, offset + region->obj_offset, &paddr);
				if(unlikely(ret != STATUS_SUCCESS)) {
					dprintf("vm:  could not read page from source (%d)\n", ret);
					mutex_unlock(&amap->lock);
					return VM_FAULT_OOM;
				}
			}

			dprintf("vm:  anon write fault: copying page 0x%" PRIxPHYS " from %p\n",
			        paddr, handle->object);

			page = page_alloc(MM_SLEEP);
			phys_copy(page->addr, paddr, MM_SLEEP);

			/* Add the page and release the old one. */
			refcount_inc(&page->count);
			amap->pages[i] = page;
			if(handle->object->type->release_page) {
				handle->object->type->release_page(handle, offset + region->obj_offset, paddr);
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
			assert(handle->object->type->get_page);

			/* Get the page from the source, and map read-only. */
			ret = handle->object->type->get_page(handle, offset + region->obj_offset, &paddr);
			if(unlikely(ret != STATUS_SUCCESS)) {
				dprintf("vm:  could not read page from source (%d)\n", ret);
				mutex_unlock(&amap->lock);
				return VM_FAULT_OOM;
			}

			dprintf("vm:  anon read fault: mapping page 0x%" PRIxPHYS " from %p as read-only\n",
			        paddr, handle->object);
			write = false;
		}
	}

	/* The page address should now be stored in paddr, and flags should be
	 * set correctly. If this is a protection fault, remove existing
	 * mappings. */
	if(reason == VM_FAULT_PROTECTION) {
		if(unlikely(!mmu_context_unmap(region->as->mmu, addr, true, NULL))) {
			fatal("Could not remove previous mapping for %p", addr);
		}
	}

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	mmu_context_map(region->as->mmu, addr, paddr, write, region->flags & VM_REGION_EXEC, MM_SLEEP);
	dprintf("vm:  anon fault: mapped 0x%" PRIxPHYS " at %p (as: %p, write: %d)\n",
	        paddr, addr, region->as, write);
	mutex_unlock(&amap->lock);
	return VM_FAULT_SUCCESS;
}

/** Handles a fault on objects requiring no special handling.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		VM_FAULT_* code describing result of the fault. */
static int vm_generic_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	phys_ptr_t phys, exist;
	bool write, exec;
	offset_t offset;
	status_t ret;

	assert(region->handle);
	assert(region->handle->object->type->get_page);

	/* Get a page from the object. */
	offset = (offset_t)(addr - region->start) + region->obj_offset;
	ret = region->handle->object->type->get_page(region->handle, offset, &phys);
	if(unlikely(ret != STATUS_SUCCESS)) {
		dprintf("vm:  failed to get page for %p (%d)\n", addr, ret);
		return VM_FAULT_OOM;
	}

	/* Check if a mapping already exists. This is possible if two threads
	 * in a process on different CPUs fault on the same address
	 * simultaneously. */
	if(mmu_context_query(region->as->mmu, addr, &exist, NULL, NULL)) {
		if(exist != phys) {
			fatal("Incorrect existing mapping found (found %" PRIxPHYS", should be %" PRIxPHYS ")",
			      exist, phys);
		} else if(region->handle->object->type->release_page) {
			region->handle->object->type->release_page(region->handle, offset, phys);
		}
		return VM_FAULT_SUCCESS;
	}

	/* Work out the flags to map with. */
	write = region->flags & VM_REGION_WRITE;
	exec = region->flags & VM_REGION_EXEC;

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	mmu_context_map(region->as->mmu, addr, phys, write, exec, MM_SLEEP);
	dprintf("vm:  mapped 0x%" PRIxPHYS " at %p (as: %p, write: %d, exec: %d)\n",
	        phys, addr, region->as, write, exec);
	return VM_FAULT_SUCCESS;
}

/** Page fault handler.
 * @param addr		Address the fault occurred at.
 * @param reason	Reason for the fault.
 * @param access	Type of memory access that caused the fault.
 * @return		VM_FAULT_* code describing result of the fault. */
int vm_fault(ptr_t addr, int reason, int access) {
	vm_aspace_t *as = curr_aspace;
	vm_region_t *region;
	int ret;

	/* If we don't have an address space, don't do anything. */
	if(unlikely(!as)) {
		return VM_FAULT_FAILURE;
	}

	assert(!(as->flags & VM_ASPACE_MLOCK));

	dprintf("vm: page fault at %p (as: %p, reason: %d, access: %d)\n", addr, as, reason, access);

	/* Round down address to a page boundary. */
	addr &= PAGE_MASK;

	/* Safe to take the lock despite us being in an interrupt - the lock
	 * is only held within the functions in this file, and they should not
	 * incur a pagefault (if they do there's something wrong!). */
	if(unlikely(mutex_held(&as->lock) && as->lock.holder == curr_thread)) {
		kprintf(LOG_WARN, "vm: recursive locking on %p, fault in VM operation?\n");
		return VM_FAULT_FAILURE;
	}

	mutex_lock(&as->lock);

	/* Find the region that the fault occurred in. */
	region = vm_region_find(as, addr, false);
	if(unlikely(!region)) {
		mutex_unlock(&as->lock);
		return VM_FAULT_NOREGION;
	}

	assert(vm_region_used(region));
	assert(region->amap || region->handle);

	/* Check whether the access is allowed. Fault codes are defined to the
	 * same value as region protection flags. */
	if(!(region->flags & access)) {
		mutex_unlock(&as->lock);
		return VM_FAULT_ACCESS;
	}

	/* If the region is a stack region, check if we've hit the guard page.
	 * TODO: Stack direction. */
	if(region->flags & VM_REGION_STACK && addr == region->start) {
		kprintf(LOG_DEBUG, "vm: thread %" PRIu32 " hit stack guard page %p\n",
		        curr_thread->id, addr);
		mutex_unlock(&as->lock);
		return VM_FAULT_NOREGION;
	}

	/* Lock the MMU context. */
	mmu_context_lock(as->mmu);

	/* Call the anonymous fault handler if there is an anonymous map, else
	 * use the generic fault handler. */
	if(region->amap) {
		ret = vm_anon_fault(region, addr, reason, access);
	} else {
		ret = vm_generic_fault(region, addr, reason, access);
	}

	mmu_context_unlock(as->mmu);
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
 * @return		Status code describing result of the operation.
 */
status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size) {
	vm_region_t *region;

	if(!size || start % PAGE_SIZE || size % PAGE_SIZE) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&as->lock);

	if(!vm_region_fits(as, start, size)) {
		mutex_unlock(&as->lock);
		return STATUS_NO_MEMORY;
	}

	region = vm_region_insert(as, start, start + size, VM_REGION_RESERVED);
	dprintf("vm: reserved region [%p,%p) (as: %p)\n",region->start, region->end, as);
	mutex_unlock(&as->lock);
	return STATUS_SUCCESS;
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
 * @return		Status code describing result of the operation.
 */
status_t vm_map(vm_aspace_t *as, ptr_t start, size_t size, int flags, object_handle_t *handle,
                offset_t offset, ptr_t *addrp) {
	vm_region_t *region;
	int rflags, access;
	status_t ret;
	ptr_t i;

	/* Check whether the supplied arguments are valid. */
	if(!size || size % PAGE_SIZE || offset % PAGE_SIZE) {
		return STATUS_INVALID_ARG;
	} else if(!(flags & (VM_MAP_READ | VM_MAP_WRITE | VM_MAP_EXEC))) {
		return STATUS_INVALID_ARG;
	}
	if(flags & VM_MAP_FIXED) {
		if(start % PAGE_SIZE) {
			return STATUS_INVALID_ARG;
		}
	} else if(!addrp) {
		return STATUS_INVALID_ARG;
	}
	if(handle) {
		/* Check for overflow. */
		if((offset + size) < offset) {
			return STATUS_INVALID_ARG;
		}

		/* Check if the object can be mapped in with the given flags. */
		if(handle->object->type->mappable) {
			assert(handle->object->type->get_page);
			ret = handle->object->type->mappable(handle, flags);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		} else if(!handle->object->type->get_page) {
			return STATUS_NOT_SUPPORTED;
		}
	}

	/* Cannot have a guard page on a 1-page stack. */
	if(flags & VM_MAP_STACK && size == PAGE_SIZE) {
		flags &= ~VM_MAP_STACK;
	}

	/* Convert mapping flags to region flags. The flags with a region
	 * equivalent have the same value. */
	rflags = flags & (VM_MAP_READ | VM_MAP_WRITE | VM_MAP_EXEC | VM_MAP_PRIVATE | VM_MAP_STACK);

	mutex_lock(&as->lock);

	/* If the mapping is fixed, remove anything in the location we want to
	 * insert into, otherwise find some free space. */
	if(flags & VM_MAP_FIXED) {
		if(!vm_region_fits(as, start, size)) {
			mutex_unlock(&as->lock);
			return STATUS_NO_MEMORY;
		}

		region = vm_region_insert(as, start, start + size, rflags);
	} else {
		region = vm_region_alloc(as, size, rflags);
		if(!region) {
			mutex_unlock(&as->lock);
			return STATUS_NO_MEMORY;
		}
	}

	/* Attach the object to the region, and create an anonymous map if
	 * creating an anonymous or private mapping. */
	if(handle) {
		region->handle = handle;
		object_handle_get(region->handle);
		region->obj_offset = offset;
	}
	if(!handle || (flags & VM_MAP_PRIVATE)) {
		region->amap = vm_amap_create(size);

		/* Should not fail to reference since it is newly created. */
		if(vm_amap_map(region->amap, 0, size) != STATUS_SUCCESS) {
			fatal("Could not reference new anonymous map");
		}
	}

	/* If on an address space locked into memory (i.e. the kernel address
	 * space, map all pages into memory allowing the full access of the
	 * region. */
	if(as->flags & VM_ASPACE_MLOCK) {
		/* The fault handling code doesn't actually care whether it's
		 * an execute, only if it's a write or not. */
		access = (region->flags & VM_REGION_WRITE) ? VM_FAULT_WRITE : VM_FAULT_READ;

		/* Fault on each page in the region. */
		mmu_context_lock(as->mmu);
		for(i = region->start; i < region->end; i += PAGE_SIZE) {
			if(region->amap) {
				ret = vm_anon_fault(region, i, VM_FAULT_NOTPRESENT, access);
			} else {
				ret = vm_generic_fault(region, i, VM_FAULT_NOTPRESENT, access);
			}

			if(unlikely(ret != VM_FAULT_SUCCESS)) {
				/* The only allowed failure is OOM, in which
				 * case we have to revert the entire mapping. */
				if(likely(ret == VM_FAULT_OOM)) {
					mmu_context_unlock(as->mmu);
					vm_region_insert(as, region->start, region->end, 0);
					mutex_unlock(&as->lock);
					return STATUS_NO_MEMORY;
				} else {
					fatal("Failed to map in kernel region (%d)", ret);
				}
			}
		}
		mmu_context_unlock(as->mmu);
	}

	dprintf("vm: mapped region [%p,%p) (as: %p, handle: %p, flags(m/r): %d/%d)\n",
	        region->start, region->end, as, handle, flags, rflags);
	if(addrp) {
		*addrp = region->start;
	}
	mutex_unlock(&as->lock);
	return STATUS_SUCCESS;
}

/** Unmaps a region of memory.
 *
 * Marks the specified address range as free in an address space and unmaps
 * anything that may be mapped there.
 *
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		Status code describing result of the operation.
 */
status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size) {
	if(!size || start % PAGE_SIZE || size % PAGE_SIZE) {
		return STATUS_INVALID_ARG;
	}

	mutex_lock(&as->lock);

	if(!vm_region_fits(as, start, size)) {
		mutex_unlock(&as->lock);
		return STATUS_NO_MEMORY;
	}

	vm_region_insert(as, start, start + size, 0);
	dprintf("vm: unmapped region [%p,%p) (as: %p)\n", start, start + size, as);
	mutex_unlock(&as->lock);
	return STATUS_SUCCESS;
}

/** Switch to another address space.
 * @note		Does not take address space lock because this function
 *			is used during rescheduling.
 * @param as		Address space to switch to. */
void vm_aspace_switch(vm_aspace_t *as) {
	bool state;

	/* The kernel process does not have an address space. When switching
	 * to one of its threads, it is not necessary to switch to the kernel
	 * MMU context, as all mappings in the kernel context are visible in
	 * all address spaces. Kernel threads should never touch the userspace
	 * portion of the address space. */
	if(as && as != curr_aspace) {
		state = intr_disable();

		/* Decrease old address space's reference count, if there is one. */
		if(curr_aspace) {
			refcount_dec(&curr_aspace->count);
		}

		/* Switch to the new address space. */
		refcount_inc(&as->count);
		mmu_context_switch(as->mmu);
		curr_aspace = as;

		intr_restore(state);
	}
}

/** Create a new address space.
 * @return		Pointer to address space structure. */
vm_aspace_t *vm_aspace_create(void) {
	vm_region_t *region;
	vm_aspace_t *as;
	status_t ret;

	as = slab_cache_alloc(vm_aspace_cache, MM_SLEEP);
	as->mmu = mmu_context_create(MM_SLEEP);
	as->flags = 0;
	as->find_cache = NULL;
	as->free_map = 0;

	/* Insert the initial free region. */
	region = vm_region_create(as, USER_MEMORY_BASE, USER_MEMORY_BASE + USER_MEMORY_SIZE, 0);
	list_append(&as->regions, &region->header);
	vm_freelist_insert(region, USER_MEMORY_SIZE);

	/* Mark the first page of the address space as reserved to catch NULL
	 * pointer accesses. Also mark the libkernel area as reserved. This
	 * should not fail. */
	ret = vm_reserve(as, 0x0, PAGE_SIZE);
	assert(ret == STATUS_SUCCESS);
	ret = vm_reserve(as, LIBKERNEL_BASE, LIBKERNEL_SIZE);
	assert(ret == STATUS_SUCCESS);
	return as;
}

/** Create a clone of an address space.
 *
 * Creates a clone of an existing address space. Non-private regions will be
 * shared among the two address spaces (modifications in one will affect both),
 * whereas private regions will be duplicated via copy-on-write.
 *
 * @todo		Duplicate all mappings from the previous address space
 *			after cloning regions, to remove overhead of faulting
 *			in the new process.
 *
 * @param orig		Original address space.
 *
 * @return		Pointer to new address space.
 */
vm_aspace_t *vm_aspace_clone(vm_aspace_t *orig) {
	vm_region_t *orig_region, *region;
	vm_aspace_t *as;

	assert(!(orig->flags & VM_ASPACE_MLOCK));

	as = slab_cache_alloc(vm_aspace_cache, MM_SLEEP);
	as->mmu = mmu_context_create(MM_SLEEP);
	as->flags = 0;
	as->find_cache = NULL;
	as->free_map = 0;

	mutex_lock(&orig->lock);

	/* Clone each region in the original address space. */
	LIST_FOREACH(&orig->regions, iter) {
		orig_region = list_entry(iter, vm_region_t, header);
		region = vm_region_clone(orig_region, as);
		list_append(&as->regions, &region->header);

		/* Insert into the region tree or the free lists. */
		if(vm_region_used(region)) {
			avl_tree_insert(&as->tree, &region->tree_link, region->start, region);
		} else if(!(region->flags & VM_REGION_RESERVED)) {
			vm_freelist_insert(region, region->end - region->start);
		}
	}

	mutex_unlock(&orig->lock);
	return as;
}

/** Switch away from an address space to the kernel MMU context. */
static inline void do_switch_aspace(vm_aspace_t *as) {
	assert(!curr_proc->aspace);
	mmu_context_switch(&kernel_mmu_context);
	refcount_dec(&as->count);
	curr_aspace = NULL;
}

#if CONFIG_SMP
/** Switch away from an address space. */
static status_t switch_aspace_ipi(void *msg, unative_t _as, unative_t arg2, unative_t arg3, unative_t arg4) {
	vm_aspace_t *as = (vm_aspace_t *)_as;

	/* We may have switched address space between the check below and
	 * receiving the IPI. Avoid an unnecessary switch in this case. */
	if(as == curr_aspace) {
		do_switch_aspace(as);
	}

	return STATUS_SUCCESS;
}
#endif

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
#if CONFIG_SMP
	cpu_t *cpu;
#endif

	assert(as);

	/* If the address space is in use, it must mean that a CPU has not
	 * switched away from it because it is now running a kernel thread
	 * (see the comment in vm_aspace_switch()). We need to go through
	 * and prod any CPUs that are using it. */
	if(refcount_get(&as->count) > 0) {
#if CONFIG_SMP
		LIST_FOREACH(&running_cpus, iter) {
			cpu = list_entry(iter, cpu_t, header);
			if(cpu->aspace == as) {
				if(cpu == curr_cpu) {
					do_switch_aspace(as);
				} else {
					ipi_send(cpu->id, switch_aspace_ipi, (unative_t)as,
					         0, 0, 0, IPI_SEND_SYNC);
				}
			}
		}
#else
		do_switch_aspace(as);
#endif
		/* The address space should no longer be in use. */
		assert(refcount_get(&as->count) == 0);
	}

	/* Unmap and destroy each region. */
	LIST_FOREACH_SAFE(&as->regions, iter) {
		vm_region_destroy(list_entry(iter, vm_region_t, header));
	}

	/* Destroy the MMU context. */
	mmu_context_destroy(as->mmu);

	assert(list_empty(&as->regions));
	assert(avl_tree_empty(&as->tree));
	slab_cache_free(vm_aspace_cache, as);
}

/** Initialise the VM system. */
void __init_text vm_init(void) {
	vm_region_t *region;

	/* Create the VM slab caches. */
	vm_aspace_cache = slab_cache_create("vm_aspace_cache", sizeof(vm_aspace_t),
	                                    0, vm_aspace_ctor, NULL, NULL, 0,
	                                    MM_FATAL);
	vm_region_cache = slab_cache_create("vm_region_cache", sizeof(vm_region_t),
	                                    0, NULL, NULL, NULL, 0, MM_FATAL);
	vm_amap_cache = slab_cache_create("vm_amap_cache", sizeof(vm_amap_t),
	                                  0, vm_amap_ctor, NULL, NULL, 0,
	                                  MM_FATAL);

	/* Initialise the other parts of the VM system. */
	vm_cache_init();

	/* Create the kernel address space. */
	kernel_aspace = slab_cache_alloc(vm_aspace_cache, MM_FATAL);
	kernel_aspace->mmu = &kernel_mmu_context;
	kernel_aspace->flags = VM_ASPACE_MLOCK;
	kernel_aspace->find_cache = NULL;
	kernel_aspace->free_map = 0;

	/* Insert the initial free region. */
	region = vm_region_create(kernel_aspace, KERNEL_VM_BASE, KERNEL_VM_BASE + KERNEL_VM_SIZE, 0);
	list_append(&kernel_aspace->regions, &region->header);
	vm_freelist_insert(region, KERNEL_VM_SIZE);
}

/** Display details of a region.
 * @param region	Region to display. */
static void dump_region(vm_region_t *region) {
	kprintf(LOG_NONE, "%-18p %-18p %-5d %-18p %-10" PRIu64 " %-18p %" PRIu64 "\n",
	        region->start, region->end, region->flags,
	        region->handle, region->obj_offset, region->amap,
	        region->amap_offset);
}

/** Dump an address space.
 * @param argc		Argument count.
 * @param argv		Argument pointer array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_aspace(int argc, char **argv) {
	process_t *process;
	vm_aspace_t *as;
	unative_t val;
	int i;

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

	LIST_FOREACH(&as->regions, iter) {
		dump_region(list_entry(iter, vm_region_t, header));
	}

	kprintf(LOG_NONE, "\nAllocated:\n\n");

	AVL_TREE_FOREACH(&as->tree, iter) {
		dump_region(avl_tree_entry(iter, vm_region_t));
	}

	for(i = 0; i < VM_FREELISTS; i++) {
		if(!(as->free_map & ((ptr_t)1 << i))) {
			if(list_empty(&as->free[i])) {
				continue;
			}
			kprintf(LOG_NONE, "\nFreelist %d (shouldn't have entries!):\n\n", i);
		} else {
			kprintf(LOG_NONE, "\nFreelist %d:\n\n", i);
		}

		LIST_FOREACH(&as->free[i], iter) {
			dump_region(list_entry(iter, vm_region_t, free_link));
		}
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
 * @param start		Start address of region (if VM_MAP_FIXED). Must be a
 *			multiple of the system page size.
 * @param size		Size of region to map. Must be a multiple of the system
 *			page size.
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param handle	Handle to object to map in. If negative, then the
 *			region will be an anonymous memory mapping.
 * @param offset	Offset into object to map from.
 * @param addrp		Where to store address of mapping.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_vm_map(void *start, size_t size, int flags, handle_t handle, offset_t offset,
                     void **addrp) {
	object_handle_t *khandle = NULL;
	status_t ret;
	ptr_t addr;

	if(!(flags & VM_MAP_FIXED) && !addrp) {
		return STATUS_INVALID_ARG;
	} else if(handle >= 0) {
		ret = object_handle_lookup(handle, -1, 0, &khandle);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
	}

	ret = vm_map(curr_proc->aspace, (ptr_t)start, size, flags, khandle, offset, &addr);
	if(ret == STATUS_SUCCESS && addrp) {
		ret = memcpy_to_user(addrp, &addr, sizeof(void *));
		if(ret != STATUS_SUCCESS) {
			vm_unmap(curr_proc->aspace, addr, size);
		}
	}

	if(khandle) {
		object_handle_release(khandle);
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
 * @return		Status code describing result of the operation.
 */
status_t kern_vm_unmap(void *start, size_t size) {
	return vm_unmap(curr_proc->aspace, (ptr_t)start, size);
}
