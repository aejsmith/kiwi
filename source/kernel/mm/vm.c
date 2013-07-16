/*
 * Copyright (C) 2009-2013 Alex Smith
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
 *    region lookups upon page faults. We do not care about free or reserved
 *    regions when doing these lookups, as a page fault on a free region is
 *    invalid, so including free regions in this tree would be an unnecessary
 *    bottleneck.
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
 *    remapped as read-write (if the region is VM_PROT_WRITE, that is).
 *  - Each object also contains an array of reference counts for each page that
 *    the object can cover. This array is used to track how many regions are
 *    mapping each page of the object, allowing pages to be freed when no more
 *    regions refer to them.
 *
 * @todo		The anonymous object page array could be changed into a
 *			two-level array, which would reduce memory consumption
 *			for large, sparsely-used objects.
 * @todo		Swap support.
 * @todo		Implement VM_MAP_INHERIT, VM_MAP_OVERCOMMIT (at the
 *			moment we just overcommit regardless).
 * @todo		Memory locking.
 * @todo		Shouldn't use MM_KERNEL for user address spaces? Note
 *			that MM_USER will at some point use interruptible sleep
 *			so interrupting a process waiting for memory leaves us
 *			no choice but to crash the thread.
 * @todo		When cloning, duplicate all mappings from the previous
 *			address space after cloning regions, to remove overhead
 *			of faulting in the new process? This may not be the best
 *			thing to do as cloning is mainly used to implement the
 *			POSIX fork() system call, which is most often followed
 *			with an exec(). In this case, the majority of the
 *			duplicated mappings would in fact not be required, and
 *			copying mappings would be more overhead. Could possibly
 *			just map a working set.
 */

#include <arch/frame.h>
#include <arch/memory.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/phys.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/vm_cache.h>

#include <proc/process.h>
#include <proc/signal.h>
#include <proc/thread.h>

#include <assert.h>
#include <kdb.h>
#include <smp.h>
#include <status.h>

#include "vm_private.h"

/** Define to enable (very) verbose debug output. */
#define DEBUG_VM

#ifdef DEBUG_VM
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab caches used for VM structures. */
static slab_cache_t *vm_aspace_cache = NULL;
static slab_cache_t *vm_region_cache = NULL;
static slab_cache_t *vm_amap_cache = NULL;

/** Constructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void vm_aspace_ctor(void *obj, void *data) {
	vm_aspace_t *as = obj;
	unsigned i;

	mutex_init(&as->lock, "vm_aspace_lock", 0);
	refcount_set(&as->count, 0);
	avl_tree_init(&as->tree);
	list_init(&as->regions);

	for(i = 0; i < VM_FREELISTS; i++)
		list_init(&as->free[i]);
}

/** Constructor for region objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void vm_region_ctor(void *obj, void *data) {
	vm_region_t *region = obj;

	list_init(&region->header);
	list_init(&region->free_link);
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
static inline bool vm_aspace_fits(vm_aspace_t *as, ptr_t start, size_t size) {
	vm_region_t *first, *last;
	ptr_t as_end, region_end;

	assert(!list_empty(&as->regions));

	/* Get the first and last regions in the address space. */
	first = list_first(&as->regions, vm_region_t, header);
	last = list_last(&as->regions, vm_region_t, header);

	as_end = last->start + last->size - 1;
	region_end = start + size - 1;
	return (region_end >= start && start >= first->start && region_end <= as_end);
}

/** Add a region to the appropriate free list.
 * @param region	Region to add.
 * @param size		Size of the region. */
static inline void vm_freelist_insert(vm_region_t *region, size_t size) {
	unsigned list = highbit(size) - PAGE_WIDTH - 1;

	assert(region->state == VM_REGION_FREE);
	list_append(&region->as->free[list], &region->free_link);
	region->as->free_map |= ((ptr_t)1 << list);
}

/** Remove a region from its free list.
 * @param region	Region to remove. */
static inline void vm_freelist_remove(vm_region_t *region) {
	unsigned list = highbit(region->size) - PAGE_WIDTH - 1;

	assert(!region->flags);
	list_remove(&region->free_link);
	if(list_empty(&region->as->free[list]))
		region->as->free_map &= ~((ptr_t)1 << list);
}

/** Check if a freelist is empty.
 * @param as		Address space to check in.
 * @param list		List number.
 * @return		Whether the list is empty. */
static inline bool vm_freelist_empty(vm_aspace_t *as, unsigned list) {
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

	map = slab_cache_alloc(vm_amap_cache, MM_KERNEL);
	refcount_set(&map->count, 1);
	map->curr_size = 0;
	map->max_size = size >> PAGE_WIDTH;
	map->pages = kcalloc(map->max_size, sizeof(*map->pages), MM_KERNEL);
	map->rref = kcalloc(map->max_size, sizeof(*map->rref), MM_KERNEL);
	dprintf("vm: created anonymous map %p (size: %zu, pages: %zu)\n", map,
		size, map->max_size);
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
			kprintf(LOG_DEBUG, "vm: anon object %p rref[%zu] is at "
				"maximum value!\n", map, i);

			/* Go and undo what we've done. */
			for(j = start; j < i; j++)
				map->rref[j]--;

			mutex_unlock(&map->lock);
			return STATUS_NO_MEMORY;
		}

		map->rref[i]++;
	}

	mutex_unlock(&map->lock);
	return STATUS_SUCCESS;
}

/** Decrease the region reference count for part of the anonymous map.
 * @param map		Map to decrease count on.
 * @param offset	Offset into the map to start from.
 * @param size		Size of the range. */
static void vm_amap_unmap(vm_amap_t *map, offset_t offset, size_t size) {
	size_t i, start, end;

	mutex_lock(&map->lock);

	/* Work out the entries within the object that this covers and ensure
	 * it's within the object - for anonymous objects mappings can't be
	 * outside the object. */
	start = (size_t)(offset >> PAGE_WIDTH);
	end = start + (size >> PAGE_WIDTH);
	assert(end <= map->max_size);

	for(i = start; i < end; i++) {
		assert(map->rref[i]);

		if(--map->rref[i] == 0 && map->pages[i]) {
			dprintf("vm: anon object %p rref[%zu] reached 0, freeing"
				"0x%" PRIxPHYS "\n", map, i, map->pages[i]->addr);

			if(refcount_dec(&map->pages[i]->count) == 0)
				page_free(map->pages[i]);

			map->pages[i] = NULL;
			map->curr_size--;
		}
	}

	mutex_unlock(&map->lock);
}

/** Get the region before another region in the region list.
 * @param region	Region to get region before from.
 * @return		Pointer to previous region, or NULL if start of list. */
static vm_region_t *vm_region_prev(vm_region_t *region) {
	if(region == list_first(&region->as->regions, vm_region_t, header))
		return NULL;

	return list_prev(&region->header, vm_region_t, header);
}

/** Get the region after another region in the region list.
 * @param region	Region to get region after from.
 * @return		Pointer to next region, or NULL if end of list. */
static vm_region_t *vm_region_next(vm_region_t *region) {
	if(region == list_last(&region->as->regions, vm_region_t, header))
		return NULL;

	return list_next(&region->header, vm_region_t, header);
}

/** Check if a region contains an address.
 * @param region	Region to check.
 * @param addr		Address to check.
 * @return		Whether the region contains the address. */
static inline bool vm_region_contains(vm_region_t *region, ptr_t addr) {
	return (addr >= region->start && addr <= (region->start + region->size - 1));
}

/** Check if two regions can be merged.
 * @param a		First region.
 * @param b		Second region.
 * @return		Whether the regions can be merged. Regions are only
 *			mergeable if unused. */
static inline bool vm_region_mergeable(const vm_region_t *a, const vm_region_t *b) {
	return (a->state != VM_REGION_ALLOCATED && a->state == b->state);
}

/** Allocate a new region structure. Caller must attach object to it.
 * @param as		Address space of the region.
 * @param start		Start address of the region.
 * @param size		Size of the region.
 * @param protection	Protection flags for the region.
 * @param flags		Flags for the region.
 * @param state		Allocation state of the region.
 * @param name		Name of the region (will be copied).
 * @return		Pointer to region structure. */
static vm_region_t *vm_region_create(vm_aspace_t *as, ptr_t start, size_t size,
	uint32_t protection, uint32_t flags, int state, const char *name)
{
	vm_region_t *region;

	region = slab_cache_alloc(vm_region_cache, MM_KERNEL);
	region->as = as;
	region->start = start;
	region->size = size;
	region->protection = protection;
	region->flags = flags;
	region->state = state;
	region->handle = NULL;
	region->obj_offset = 0;
	region->amap = NULL;
	region->amap_offset = 0;
	region->name = (name) ? kstrdup(name, MM_KERNEL) : NULL;
	return region;
}

/** Clone a region.
 * @param src		Region to clone.
 * @param as		Address space for new region.
 * @return		Pointer to cloned region. */
static vm_region_t *vm_region_clone(vm_region_t *src, vm_aspace_t *as) {
	size_t i, start, end;
	vm_region_t *dest;

	dest = vm_region_create(as, src->start, src->size, src->protection,
		src->flags, src->state, src->name);
	if(src->state != VM_REGION_ALLOCATED)
		return dest;

	/* Copy the object handle. */
	if(src->handle) {
		object_handle_retain(src->handle);
		dest->handle = src->handle;
		dest->obj_offset = src->obj_offset;
	}

	/* If this is not a private mapping, just point the new region at the
	 * old anonymous map and return. */
	if(!(src->flags & VM_MAP_PRIVATE)) {
		if(src->amap) {
			refcount_inc(&src->amap->count);
			vm_amap_map(src->amap, src->amap_offset, src->size);
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
	start = src->amap_offset >> PAGE_WIDTH;
	end = start + (src->size >> PAGE_WIDTH);
	assert(end <= src->amap->max_size);

	/* Create a new map. */
	dest->amap = vm_amap_create(src->size);

	/* Write-protect all mappings on the source region. */
	mmu_context_lock(src->as->mmu);
	mmu_context_protect(src->as->mmu, src->start, src->size,
		(src->protection & VM_PROT_EXECUTE) ? MMU_MAP_EXECUTE : 0);
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

/** Searches for a region containing an address.
 * @param as		Address space to search in (should be locked).
 * @param addr		Address to search for.
 * @param unused	Whether to include unused regions in the search.
 * @return		Pointer to region if found, false if not. If including
 *			unused regions, this will always succeed unless the
 *			given address is invalid. */
static vm_region_t *vm_region_find(vm_aspace_t *as, ptr_t addr, bool unused) {
	avl_tree_node_t *node;
	vm_region_t *region, *near = NULL;

	/* Check if the cached pointer matches. Caching the last found region
	 * helps mainly for page fault handling when code is hitting different
	 * parts of a newly-mapped region in succession. */
	if(as->find_cache && vm_region_contains(as->find_cache, addr))
		return as->find_cache;

	/* Fall back on searching through the AVL tree. */
	node = as->tree.root;
	while(node) {
		region = avl_tree_entry(node, vm_region_t, tree_link);
		assert(region->state == VM_REGION_ALLOCATED);
		if(addr >= region->start) {
			if(vm_region_contains(region, addr)) {
				as->find_cache = region;
				return region;
			}

			near = avl_tree_entry(node, vm_region_t, tree_link);
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
			region = vm_region_next(near);
		} else {
			/* Should never be empty. */
			assert(!list_empty(&as->regions));
			region = list_first(&as->regions, vm_region_t, header);
		}

		while(region) {
			if(vm_region_contains(region, addr)) {
				assert(region->state != VM_REGION_ALLOCATED);
				return region;
			}

			region = vm_region_next(region);
		}
	}

	return NULL;
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
 * @param size		Size of range to unmap. */
static void vm_region_unmap(vm_region_t *region, ptr_t start, size_t size) {
	phys_ptr_t phys;
	offset_t offset;
	size_t i, idx;

	assert(region->state == VM_REGION_ALLOCATED);
	assert(region->handle || region->amap);

	mmu_context_lock(region->as->mmu);

	for(i = 0; i < size; i += PAGE_SIZE) {
		offset = (start - region->start) + i;

		/* Unmap the page and release it from its source. */
		if(mmu_context_unmap(region->as->mmu, start + i, true, &phys)) {
			if(region->amap) {
				offset += region->amap_offset;
				idx = offset >> PAGE_WIDTH;

				assert(idx < region->amap->max_size);

				/* If page is in the object, then do nothing. */
				if(region->amap->pages[idx]) {
					assert(region->amap->pages[idx]->addr == phys);
					return;
				}

				assert(region->handle);
				assert(region->handle->object->type->release_page);
			}

			if(region->handle->object->type->release_page) {
				offset += region->obj_offset;
				region->handle->object->type->release_page(
					region->handle, offset, phys);
			}
		}
	}

	mmu_context_unlock(region->as->mmu);

	/* Release the pages in the anonymous map. */
	if(region->amap) {
		offset = (start - region->start) + region->amap_offset;
		vm_amap_unmap(region->amap, offset, size);
	}
}

/** Unmap an entire region.
 * @param region	Region to destroy. */
static void vm_region_destroy(vm_region_t *region) {
	/* Unmap the region and drop references to the object/anonymous map,
	 * and remove it from the tree or freelist. */
	if(region->state == VM_REGION_ALLOCATED) {
		vm_region_unmap(region, region->start, region->size);

		if(region->amap)
			vm_amap_release(region->amap);
		if(region->handle)
			object_handle_release(region->handle);

		avl_tree_remove(&region->as->tree, &region->tree_link);
	} else if(region->state == VM_REGION_FREE) {
		vm_freelist_remove(region);
	}

	/* Remove from the main region list. */
	list_remove(&region->header);

	/* If the region was the cached find pointer, get rid of it. */
	if(region == region->as->find_cache)
		region->as->find_cache = NULL;

	assert(list_empty(&region->free_link));
	slab_cache_free(vm_region_cache, region);
}

/** Handle an fault on a region with an anonymous map.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		Status code describing the result of the operation. */
static status_t vm_anon_fault(vm_region_t *region, ptr_t addr, int reason, uint32_t access) {
	object_handle_t *handle = region->handle;
	vm_amap_t *amap = region->amap;
	phys_ptr_t phys;
	offset_t offset;
	page_t *page;
	status_t ret;
	size_t idx;
	unsigned protect;

	/* Work out the offset into the object. */
	offset = region->amap_offset + (addr - region->start);
	idx = (size_t)(offset >> PAGE_WIDTH);

	mutex_lock(&amap->lock);

	assert(idx < amap->max_size);

	/* Do some sanity checks if this is a protection fault. The main fault
	 * handler verifies that the access is allowed by the region protection,
	 * so the only access type protection faults should be is write. COW
	 * faults should never occur on non-private regions, either. */
	if(reason == VM_FAULT_PROTECTION) {
		if(access != VM_PROT_WRITE) {
			fatal("Non-write protection fault at %p on %p (%d)",
				addr, amap, access);
		} else if(!(region->flags & VM_MAP_PRIVATE)) {
			fatal("Copy-on-write fault at %p on non-private region",
				addr);
		}
	}

	/* Protection flags to map with. The write flag is cleared later on if
	 * the page needs to be mapped read only. */
	protect = 0;
	if(region->protection & VM_PROT_WRITE)
		protect |= MMU_MAP_WRITE;
	if(region->protection & VM_PROT_EXECUTE)
		protect |= MMU_MAP_EXECUTE;

	if(!amap->pages[idx] && !handle) {
		/* No page existing and no source. Allocate a zeroed page. */
		dprintf("vm:  anon fault: no existing page and no source, allocating new\n");
		amap->pages[idx] = page_alloc(MM_KERNEL | MM_ZERO);
		refcount_inc(&amap->pages[idx]->count);
		amap->curr_size++;
		phys = amap->pages[idx]->addr;
	} else if(access == VM_PROT_WRITE) {
		if(amap->pages[idx]) {
			assert(refcount_get(&amap->pages[idx]->count) > 0);

			/* If the reference count is greater than 1 we must
			 * copy it. Shared regions should not contain any pages
			 * with a reference count greater than 1. */
			if(refcount_get(&amap->pages[idx]->count) > 1) {
				assert(region->flags & VM_MAP_PRIVATE);

				dprintf("vm:  anon write fault: copying page %zu "
					"(addr: 0x%" PRIxPHYS ", refcount: %" 
					PRId32 ")\n", idx, amap->pages[idx]->addr,
					amap->pages[idx]->count);

				page = page_copy(amap->pages[idx], MM_KERNEL);
				refcount_inc(&page->count);

				/* Decrease the count of the old page. We must
				 * handle it going to 0 here, as another object
				 * could have released the page while we were
				 * copying. */
				if(refcount_dec(&amap->pages[idx]->count) == 0)
					page_free(amap->pages[idx]);

				amap->pages[idx] = page;
			}
			
			phys = amap->pages[idx]->addr;
		} else {
			assert(region->flags & VM_MAP_PRIVATE);
			assert(handle);

			/* Find the page to copy. If handling a protection
			 * fault, use the existing mapping address. */
			if(reason == VM_FAULT_PROTECTION) {
				if(!mmu_context_query(region->as->mmu, addr, &phys, NULL)) {
					fatal("No mapping for %p, but protection "
						"fault on it", addr);
				}
			} else {
				assert(handle->object->type->get_page);

				ret = handle->object->type->get_page(handle,
					offset + region->obj_offset, &phys);
				if(ret != STATUS_SUCCESS) {
					dprintf("vm: failed to get page at offset 0x%"
						PRIx64 " from %p (object: %p): %d\n",
						offset + region->obj_offset,
						handle, handle->object, ret);
					mutex_unlock(&amap->lock);
					return ret;
				}
			}

			dprintf("vm:  anon write fault: copying page 0x%" PRIxPHYS
				" from %p (object: %p)\n", phys, handle,
				handle->object);

			page = page_alloc(MM_KERNEL);
			phys_copy(page->addr, phys, MM_KERNEL);

			/* Add the page and release the old one. */
			refcount_inc(&page->count);
			amap->pages[idx] = page;
			if(handle->object->type->release_page) {
				handle->object->type->release_page(handle,
					offset + region->obj_offset, phys);
			}

			amap->curr_size++;
			phys = page->addr;
		}
	} else {
		if(amap->pages[idx]) {
			assert(refcount_get(&amap->pages[idx]->count) > 0);

			/* If the reference count is greater than 1, map read
			 * only so we copy it if there is a later write to the
			 * page. */
			if(refcount_get(&amap->pages[idx]->count) > 1) {
				assert(region->flags & VM_MAP_PRIVATE);
				protect &= ~MMU_MAP_WRITE;
			}

			phys = amap->pages[idx]->addr;
		} else {
			assert(region->flags & VM_MAP_PRIVATE);
			assert(handle);
			assert(handle->object->type->get_page);

			/* Get the page from the source, and map read-only. */
			ret = handle->object->type->get_page(handle,
				offset + region->obj_offset, &phys);
			if(ret != STATUS_SUCCESS) {
				dprintf("vm: failed to get page at offset 0x%"
					PRIx64 " from %p (object: %p): %d\n",
					offset + region->obj_offset,
					handle, handle->object, ret);
				mutex_unlock(&amap->lock);
				return ret;
			}

			dprintf("vm:  anon read fault: mapping page 0x%" PRIxPHYS
				" from %p (object: %p) as read-only\n", phys,
				handle, handle->object);

			protect &= ~MMU_MAP_WRITE;
		}
	}

	/* The page address should now be stored in phys, and protection flags
	 * should be set correctly. If this is a protection fault, remove the
	 * existing mapping. */
	if(reason == VM_FAULT_PROTECTION) {
		if(!mmu_context_unmap(region->as->mmu, addr, true, NULL))
			fatal("Could not remove previous mapping for %p", addr);
	}

	/* Map the entry in. Should always succeed with MM_KERNEL set. */
	mmu_context_map(region->as->mmu, addr, phys, protect, MM_KERNEL);

	dprintf("vm: mapped 0x%" PRIxPHYS " at %p (as: %p, protect: 0x%x)\n",
		phys, addr, region->as, protect);
	mutex_unlock(&amap->lock);
	return STATUS_SUCCESS;
}

/** Handle an fault on regions without an anonymous map.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @return		Status code describing the result of the operation. */
static status_t vm_object_fault(vm_region_t *region, ptr_t addr) {
	phys_ptr_t phys, exist;
	unsigned protect;
	offset_t offset;
	status_t ret;

	assert(region->handle);
	assert(region->handle->object->type->get_page);

	/* Get a page from the object. */
	offset = (offset_t)(addr - region->start) + region->obj_offset;
	ret = region->handle->object->type->get_page(region->handle, offset, &phys);
	if(ret != STATUS_SUCCESS) {
		dprintf("vm: failed to get page at offset 0x%" PRIx64 " from %p "
			"(object: %p): %d\n", offset, region->handle,
			region->handle->object, ret);
		return ret;
	}

	/* Check if a mapping already exists. This is possible if two threads
	 * in a process on different CPUs fault on the same address
	 * simultaneously. */
	if(mmu_context_query(region->as->mmu, addr, &exist, NULL)) {
		if(exist != phys) {
			fatal("Incorrect existing mapping found (found 0x%"
				PRIxPHYS ", should be 0x%" PRIxPHYS ")",
				exist, phys);
		} else if(region->handle->object->type->release_page) {
			region->handle->object->type->release_page(region->handle,
				offset, phys);
		}

		return STATUS_SUCCESS;
	}

	/* Work out the protection flags to map with. */
	protect = 0;
	if(region->protection & VM_PROT_WRITE)
		protect |= MMU_MAP_WRITE;
	if(region->protection & VM_PROT_EXECUTE)
		protect |= MMU_MAP_EXECUTE;

	/* Map the entry in. FIXME: Once page reservations are implemented we
	 * should reserve pages right at the beginning of the fault handler
	 * before locking the address space, as if pages need to be reclaimed
	 * we could run into issues because we're holding the address space and
	 * context locks. */
	mmu_context_map(region->as->mmu, addr, phys, protect, MM_KERNEL);

	dprintf("vm: mapped 0x%" PRIxPHYS " at %p (as: %p, protect: 0x%x)\n",
		phys, addr, region->as, protect);
	return STATUS_SUCCESS;
}

/** Page fault handler.
 * @param addr		Address the fault occurred at.
 * @param reason	Reason for the fault.
 * @param access	Type of memory access that caused the fault.
 * @return		STATUS_SUCCESS if the fault has been handled in some
 *			way, other status code if not. */
status_t vm_fault(intr_frame_t *frame, ptr_t addr, int reason, uint32_t access) {
	vm_aspace_t *as = curr_aspace;
	ptr_t base;
	vm_region_t *region;
	siginfo_t info;
	status_t ret;

	assert(!local_irq_state());

	dprintf("vm: %s-mode page fault at %p (thread: %" PRId32 ", as: %p, reason: %d, "
		"access: 0x%" PRIx32 ")\n",
		(intr_frame_from_user(frame)) ? "user" : "kernel", addr,
		curr_thread->id, as, reason, access);

	/* If we don't have an address space, don't do anything. There won't be
	 * anything to send a signal to, either. */
	if(unlikely(!as))
		return STATUS_INVALID_ADDR;

	/* Safe to take the lock despite us being in an interrupt - the lock
	 * is only held within the functions in this file, and they should not
	 * incur a page fault (if they do there's something wrong!). */
	if(unlikely(mutex_held(&as->lock) && as->lock.holder == curr_thread)) {
		kprintf(LOG_WARN, "vm: recursive locking on %p, fault in VM operation?\n");
		return STATUS_INVALID_ADDR;
	}

	mutex_lock(&as->lock);

	/* Round down address to a page boundary. */
	base = addr & PAGE_MASK;

	/* Find the region that the fault occurred in. */
	region = vm_region_find(as, base, false);
	if(unlikely(!region)) {
		kprintf(LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p: "
			"no region found\n", curr_thread->id, curr_thread->name,
			addr);
		ret = STATUS_INVALID_ADDR;
		goto out;
	}

	assert(region->state == VM_REGION_ALLOCATED);
	assert(region->amap || region->handle);

	/* Check whether the access is allowed. */
	if(!(region->protection & access)) {
		kprintf(LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p: "
			"access violation\n", curr_thread->id, curr_thread->name,
			addr);
		ret = STATUS_ACCESS_DENIED;
		goto out;
	}

	/* If the region is a stack region, check if we've hit the guard page.
	 * TODO: Stack direction. */
	if(region->flags & VM_MAP_STACK && base == region->start) {
		kprintf(LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p: "
			"hit stack guard page\n", curr_thread->id, curr_thread->name,
			addr);
		ret = STATUS_INVALID_ADDR;
		goto out;
	}

	mmu_context_lock(as->mmu);

	local_irq_enable();

	/* Call the anonymous fault handler if there is an anonymous map, else
	 * use the object fault handler. */
	ret = (region->amap) ? vm_anon_fault(region, base, reason, access)
		: vm_object_fault(region, base);

	local_irq_disable();

	mmu_context_unlock(as->mmu);

	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_NOTICE, "vm: thread %" PRId32 " (%s) page fault at %p "
			"returned %d\n", curr_thread->id, curr_thread->name,
			addr, ret);
	}
out:
	mutex_unlock(&as->lock);

	if(ret != STATUS_SUCCESS && intr_frame_from_user(frame)) {
		kdb_enter(KDB_REASON_USER, frame);

		/* Send a signal to the thread. */
		memset(&info, 0, sizeof(info));
		info.si_addr = (void *)frame->ip;
		switch(ret) {
		case STATUS_INVALID_ADDR:
			info.si_signo = SIGSEGV;
			info.si_code = SEGV_MAPERR;
			break;
		case STATUS_ACCESS_DENIED:
			info.si_signo = SIGSEGV;
			info.si_code = SEGV_ACCERR;
			break;
		default:
			info.si_signo = SIGBUS;
			info.si_code = BUS_ADRERR;
			break;
		}

		signal_send(curr_thread, info.si_signo, &info, true);
		ret = STATUS_SUCCESS;
	}

	return ret;
}

/** Cut out the specified space from the address space.
 * @param as		Address space to trim.
 * @param start		Start address of the region to trim.
 * @param size		Size of region to trim.
 * @return		Pointer to region preceding the trimmed area, NULL if
 *			there is no preceding region. */
static vm_region_t *trim_regions(vm_aspace_t *as, ptr_t start, size_t size) {
	vm_region_t *region, *prev, *next, *split;
	ptr_t end, region_end, match_start, match_end, new_start;
	size_t new_size;

	end = start + size - 1;

	/* Find the region containing the start address. */
	next = vm_region_find(as, start, true);
	assert(next);

	/* Save the region to return to the caller. If the start address matches
	 * the start of the area to trim we need to return the region before
	 * it. */
	prev = (next->start == start) ? vm_region_prev(next) : next;

	while(next) {
		region = next;
		next = vm_region_next(region);

		/* Calculate the overlapping part of the region. If there is
		 * none, we can finish. */
		region_end = region->start + region->size - 1;
		match_start = MAX(start, region->start);
		match_end = MIN(end, region_end);
		if(match_end <= match_start)
			break;

		/* If we match the whole region, destroy it and move on. */
		if(match_start == region->start && match_end == region_end) {
			vm_region_destroy(region);
			continue;
		}

		/* Unmap pages in the area we're not going to cover any more. */
		if(region->state == VM_REGION_ALLOCATED)
			vm_region_unmap(region, match_start, match_end - match_start + 1);

		if(match_start == region->start) {
			/* Cutting out from the start of the region. */
			new_start = match_end + 1;
			new_size = region_end - match_end;
		} else if(match_end == region_end) {
			/* Cutting out from the end of the region. */
			new_start = region->start;
			new_size = match_start - region->start;
		} else {
			/* Cutting out from the middle of the region. In this
			 * case we must split the region. Existing region
			 * becomes the bottom half of the split, we create a
			 * new one for the top half. */
			new_start = region->start;
			new_size = match_start - region->start;

			split = vm_region_create(as, match_end + 1,
				region_end - match_end, region->protection,
				region->flags, region->state, region->name);

			if(split->state == VM_REGION_ALLOCATED) {
				/* Copy object details into the split. */
				if((split->handle = region->handle))
					object_handle_retain(split->handle);

				if((split->amap = region->amap)) {
					refcount_inc(&split->amap->count);
					split->obj_offset = region->obj_offset;
					split->amap_offset = region->amap_offset
						+ (split->start - region->start);
				} else {
					split->obj_offset = region->obj_offset
						+ (split->start - region->start);
				}

				/* Insert the split region to the tree. */
				avl_tree_insert(&as->tree, split->start, &split->tree_link);
			} else if(split->state == VM_REGION_FREE) {
				/* Insert the split region to the free list. */
				vm_freelist_insert(split, split->size);
			}

			/* Put the split after the region in the list, then on
			 * next iteration we can break because we won't overlap
			 * any of the next region. */
			list_add_after(&region->header, &split->header);
			next = NULL;
		}

		if(new_start != region->start && region->state == VM_REGION_ALLOCATED) {
			/* Reinsert into the tree with the new start. */
			avl_tree_remove(&as->tree, &region->tree_link);
			avl_tree_insert(&as->tree, new_start, &region->tree_link);

			/* Increase the object offsets. */
			if(region->amap) {
				region->amap_offset += new_start - region->start;
			} else {
				region->obj_offset += new_start - region->start;
			}
		}

		if(new_size != region->size && region->state == VM_REGION_FREE) {
			/* Size changed, move to the correct free list. */
			vm_freelist_remove(region);
			vm_freelist_insert(region, new_size);
		}

		region->start = new_start;
		region->size = new_size;
	}

	return prev;
}

/** Insert a region, replacing overlapping existing regions.
 * @note		Addresses must be checked for validity by the caller.
 * @param as		Address space to insert into.
 * @param start		Start of region to insert.
 * @param size		Size of region to insert.
 * @param protection	Protection flags for the region.
 * @param flags		Flags for the region.
 * @param state		State of the region.
 * @param name		Name of the region (should be NULL for non-allocated
 *			regions).
 * @return		Pointer to inserted region. May not start or end at the
 *			requested addresses if inserting a free or reserved
 *			region due to coalescing. This region will have been
 *			inserted into the tree or free lists as necessary. */
static vm_region_t *insert_region(vm_aspace_t *as, ptr_t start, size_t size,
	uint32_t protection, uint32_t flags, int state, const char *name)
{
	vm_region_t *region, *exist;

	assert(state == VM_REGION_ALLOCATED || !name);

	/* Create the new region. */
	region = vm_region_create(as, start, size, protection, flags, state, name);

	/* Create a hole to insert the new region into. */
	exist = trim_regions(as, start, size);
	if(exist) {
		assert((exist->start + exist->size) == start);

		list_add_after(&exist->header, &region->header);

		/* Merge adjacent unused regions. */
		if(vm_region_mergeable(region, exist)) {
			region->start = exist->start;
			region->size += exist->size;
			vm_region_destroy(exist);
		}
	} else {
		list_prepend(&as->regions, &region->header);
	}

	/* Check if we can merge with the region after. */
	exist = vm_region_next(region);
	if(exist) {
		assert(exist->start == (start + size));

		if(vm_region_mergeable(region, exist)) {
			region->size += exist->size;
			vm_region_destroy(exist);
		}
	}

	/* Finally, insert into the region tree or the free lists. */
	if(region->state == VM_REGION_ALLOCATED) {
		avl_tree_insert(&as->tree, region->start, &region->tree_link);
	} else if(region->state == VM_REGION_FREE) {
		vm_freelist_insert(region, region->size);
	}

	return region;
}

/** Allocate space in an address space.
 * @param as		Address space to allocate in (should be locked).
 * @param size		Size of space required.
 * @param protection	Protection flags for the region.
 * @param flags		Flags for the region.
 * @param name		Name of the region.
 * @return		Pointer to region if allocated, NULL if not. */
static vm_region_t *alloc_region(vm_aspace_t *as, size_t size, uint32_t protection,
	uint32_t flags, const char *name)
{
	vm_region_t *region, *split;
	unsigned list, i;

	assert(size);

	/* Get the list to search on. If the size is exactly a power of 2, then
	 * regions on freelist[n] are guaranteed to be big enough. Otherwise,
	 * use freelist[n + 1] so that we ensure that all regions we find are
	 * large enough. However, only do this if there are available regions
	 * in higher lists. */
	list = highbit(size) - PAGE_WIDTH - 1;
	if((size & (size - 1)) != 0 && as->free_map >> (list + 1))
		list++;

	/* Find a free region. */
	for(i = list; i < VM_FREELISTS; i++) {
		if(vm_freelist_empty(as, i))
			continue;

		LIST_FOREACH(&as->free[i], iter) {
			region = list_entry(iter, vm_region_t, free_link);

			assert(region->state == VM_REGION_FREE);

			if(region->size < size)
				continue;

			vm_freelist_remove(region);

			/* If the region is too big we need to split it. This
			 * is simple: the region is free, so we don't need to
			 * deal with copying object details. */
			if(region->size > size) {
				split = vm_region_create(as, region->start + size,
					region->size - size, 0, 0, VM_REGION_FREE,
					NULL);

				/* Add the split to the lists. */
				vm_freelist_insert(split, split->size);
				list_add_after(&region->header, &split->header);

				/* Change the size of the old region. */
				region->size = size;
			}

			/* Set region state and add to the tree. */
			region->protection = protection;
			region->flags = flags;
			region->state = VM_REGION_ALLOCATED;
			region->name = (name) ? kstrdup(name, MM_KERNEL) : NULL;
			avl_tree_insert(&as->tree, region->start, &region->tree_link);

			dprintf("vm: allocated region [%p,%p) from list %u in %p\n",
				region->start, region->start + region->size, i,
				as);
			return region;
		}
	}

	return NULL;
}

/**
 * Map an object into memory.
 *
 * Creates a new memory mapping that maps either an object or anonymous memory.
 * The spec argument controls where the mapping will be placed. The following
 * address specifications are currently defined:
 *
 *  - VM_ADDRESS_ANY: The mapping can be placed anywhere available in the
 *    address space, an unused space will be allocated to fit it in.
 *  - VM_ADDRESS_EXACT: The mapping will be placed at exactly the address
 *    specified, and any existing mappings in the same region will be replaced.
 *
 * The flags argument controls the behaviour of the mapping. The following flags
 * are currently defined:
 *
 *  - VM_MAP_PRIVATE: Modifications to the mapping will not be transferred
 *    through to the source object, and if the address space is duplicated, the
 *    duplicate and original will be given copy-on-write copies of the region.
 *    If this flag is not specified and the address space is duplicated, changes
 *    made in either address space will be visible in the other.
 *  - VM_MAP_OVERCOMMIT: Memory will not be reserved for the mapping, meaning
 *    it can be made larger than the total memory available (memory is only
 *    allocated when it is actually accessed). The default behaviour is to only
 *    allow mappings if the memory requirement can be satisfied.
 *  - VM_MAP_INHERIT: When a child process is created via kern_process_create()
 *    or the current process is replaced via kern_process_replace(), the
 *    mapping will be duplicated into the new address space, using the semantics
 *    specified above for VM_MAP_PRIVATE. This can be used to pass data to
 *    child processes.
 *
 * When mapping an object, the calling process must have the correct access
 * rights to the object for the mapping permissions requested.
 *
 * @param as		Address space to map in.
 * @param addrp		For VM_ADDRESS_ANY, points to a variable in which to
 *			store the allocated address. For VM_ADDRESS_EXACT,
 *			points to a variable containing the address to place
 *			the mapping at/
 * @param size		Size of mapping (multiple of page size).
 * @param spec		Address specification (VM_ADDRESS_*).
 * @param protection	Memory protection flags (VM_PROT_*).
 * @param flags		Mapping behaviour flags (VM_MAP_*).
 * @param handle	Handle to object to map in. If NULL, then the region
 *			will be an anonymous memory mapping.
 * @param offset	Offset into object to map from (multiple of page size).
 * @param name		Name of the memory mapping, for informational purposes.
 *			Can be NULL.
 *
 * @return		Status code describing result of the operation.
 */
status_t vm_map(vm_aspace_t *as, ptr_t *addrp, size_t size, unsigned spec,
	uint32_t protection, uint32_t flags, object_handle_t *handle,
	offset_t offset, const char *name)
{
	vm_region_t *region = NULL;
	ptr_t addr;
	status_t ret;

	assert(addrp);

	if(!size || size % PAGE_SIZE) {
		return STATUS_INVALID_ARG;
	} else if(!protection) {
		return STATUS_INVALID_ARG;
	}

	addr = *addrp;

	switch(spec) {
	case VM_ADDRESS_ANY:
		break;
	case VM_ADDRESS_EXACT:
		if(addr % PAGE_SIZE || addr + size < addr)
			return STATUS_INVALID_ARG;
		break;
	default:
		return STATUS_INVALID_ARG;
	}

	if(handle) {
		if(offset % PAGE_SIZE || (offset_t)(offset + size) < offset)
			return STATUS_INVALID_ARG;

		/* Check if the object can be mapped with the requested access. */
		if(handle->object->type->mappable) {
			assert(handle->object->type->get_page);
			ret = handle->object->type->mappable(handle, protection, flags);
			if(ret != STATUS_SUCCESS)
				return ret;
		} else if(!handle->object->type->get_page) {
			return STATUS_NOT_SUPPORTED;
		}
	}

	/* Cannot have a guard page on a 1-page stack. */
	if(flags & VM_MAP_STACK && size == PAGE_SIZE)
		flags &= ~VM_MAP_STACK;

	mutex_lock(&as->lock);

	/* Create the region according to the address specification. */
	switch(spec) {
	case VM_ADDRESS_ANY:
		/* Allocate some space. */
		region = alloc_region(as, size, protection, flags, name);
		if(!region) {
			mutex_unlock(&as->lock);
			return STATUS_NO_MEMORY;
		}

		break;
	case VM_ADDRESS_EXACT:
		if(!vm_aspace_fits(as, addr, size)) {
			mutex_unlock(&as->lock);
			return STATUS_NO_MEMORY;
		}

		/* Replace any existing mappings. */
		region = insert_region(as, addr, size, protection, flags,
			VM_REGION_ALLOCATED, name);
		break;
	}

	/* Attach the object to the region. */
	if(handle) {
		region->handle = handle;
		object_handle_retain(region->handle);
		region->obj_offset = offset;
	}

	/* For private or anonymous mappings we must create an anonymous map. */
	if(!handle || flags & VM_MAP_PRIVATE) {
		region->amap = vm_amap_create(size);
		vm_amap_map(region->amap, 0, size);
	}

	dprintf("vm: mapped region [%p,%p) in %p (spec: %u, protection: 0x%"
		PRIx32 ", flags: 0x%" PRIx32 ", handle: %p, offset: 0x%" PRIx64
		")\n", region->start, region->start + region->size, as, spec,
		protection, flags, handle, offset);

	*addrp = region->start;
	mutex_unlock(&as->lock);
	return STATUS_SUCCESS;
}

/**
 * Unmaps a region of memory.
 *
 * Marks the specified address range as free in an address space and unmaps
 * anything that may be mapped there.
 *
 * @param as		Address space to unmap from.
 * @param start		Start of region to free.
 * @param size		Size of region to free.
 *
 * @return		Status code describing result of the operation.
 */
status_t vm_unmap(vm_aspace_t *as, ptr_t start, size_t size) {
	if(!size || start % PAGE_SIZE || size % PAGE_SIZE)
		return STATUS_INVALID_ARG;

	mutex_lock(&as->lock);

	if(!vm_aspace_fits(as, start, size)) {
		mutex_unlock(&as->lock);
		return STATUS_NO_MEMORY;
	}

	insert_region(as, start, size, 0, 0, VM_REGION_FREE, NULL);

	dprintf("vm: unmapped region [%p,%p) in %p\n", start, start + size, as);
	mutex_unlock(&as->lock);
	return STATUS_SUCCESS;
}

/**
 * Mark a region as reserved.
 *
 * Marks a region of memory in an address space as reserved. Reserved regions
 * will never be allocated from for VM_ADDRESS_ANY mappings, but they can be
 * overwritten by VM_ADDRESS_EXACT or removed by using vm_unmap() on them.
 *
 * @param as		Address space to reserve in.
 * @param start		Start of region to reserve.
 * @param size		Size of region to reserve.
 *
 * @return		Status code describing result of the operation.
 */
status_t vm_reserve(vm_aspace_t *as, ptr_t start, size_t size) {
	if(!size || start % PAGE_SIZE || size % PAGE_SIZE)
		return STATUS_INVALID_ARG;

	mutex_lock(&as->lock);

	if(!vm_aspace_fits(as, start, size)) {
		mutex_unlock(&as->lock);
		return STATUS_NO_MEMORY;
	}

	insert_region(as, start, size, 0, 0, VM_REGION_RESERVED, NULL);

	dprintf("vm: reserved region [%p,%p) in %p\n", start, start + size, as);
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
		state = local_irq_disable();

		/* Decrease old address space's reference count, if there is one. */
		if(curr_aspace) {
			mmu_context_unload(curr_aspace->mmu);
			refcount_dec(&curr_aspace->count);
		}

		/* Switch to the new address space. */
		refcount_inc(&as->count);
		mmu_context_load(as->mmu);
		curr_aspace = as;

		local_irq_restore(state);
	}
}

/** Create a new address space.
 * @return		Pointer to address space structure. */
vm_aspace_t *vm_aspace_create(void) {
	vm_region_t *region;
	vm_aspace_t *as;
	status_t ret;

	as = slab_cache_alloc(vm_aspace_cache, MM_KERNEL);
	as->mmu = mmu_context_create(MM_KERNEL);
	as->find_cache = NULL;
	as->free_map = 0;

	/* Insert the initial free region. */
	region = vm_region_create(as, USER_BASE, USER_SIZE, 0, 0, VM_REGION_FREE, NULL);
	list_append(&as->regions, &region->header);
	vm_freelist_insert(region, USER_SIZE);

	/* Mark the first page of the address space as reserved to catch NULL
	 * pointer accesses. Also mark the libkernel area as reserved. This
	 * should not fail. */
	ret = vm_reserve(as, 0x0, PAGE_SIZE);
	assert(ret == STATUS_SUCCESS);
	ret = vm_reserve(as, LIBKERNEL_BASE, LIBKERNEL_SIZE);
	assert(ret == STATUS_SUCCESS);
	return as;
}

/**
 * Create a clone of an address space.
 *
 * Creates a clone of an existing address space. Non-private regions will be
 * shared among the two address spaces (modifications in one will affect both),
 * whereas private regions will be duplicated via copy-on-write.
 *
 * @param orig		Original address space.
 *
 * @return		Pointer to new address space.
 */
vm_aspace_t *vm_aspace_clone(vm_aspace_t *orig) {
	vm_region_t *orig_region, *region;
	vm_aspace_t *as;

	as = slab_cache_alloc(vm_aspace_cache, MM_KERNEL);
	as->mmu = mmu_context_create(MM_KERNEL);
	as->find_cache = NULL;
	as->free_map = 0;

	mutex_lock(&orig->lock);

	/* Clone each region in the original address space. */
	LIST_FOREACH(&orig->regions, iter) {
		orig_region = list_entry(iter, vm_region_t, header);
		region = vm_region_clone(orig_region, as);
		list_append(&as->regions, &region->header);

		/* Insert into the region tree or the free lists. */
		if(region->state == VM_REGION_ALLOCATED) {
			avl_tree_insert(&as->tree, region->start, &region->tree_link);
		} else if(region->state == VM_REGION_FREE) {
			vm_freelist_insert(region, region->size);
		}
	}

	mutex_unlock(&orig->lock);
	return as;
}

/** Switch away from an address space to the kernel MMU context.
 * @param as		Address space being switched from. */
static status_t switch_to_kernel(void *_as) {
	vm_aspace_t *as = _as;
	bool state;

	state = local_irq_disable();

	/* We may have switched address space between the check below and
	 * receiving the interrupt. Avoid an unnecessary switch in this
	 * case. */
	if(as == curr_aspace) {
		mmu_context_unload(as->mmu);
		refcount_dec(&as->count);

		mmu_context_load(&kernel_mmu_context);
		curr_aspace = NULL;
	}

	local_irq_restore(state);
	return STATUS_SUCCESS;
}

/**
 * Destroy an address space.
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
					switch_to_kernel(as);
				} else {
					smp_call_single(cpu->id, switch_to_kernel, as, 0);
				}
			}
		}
		#else
		switch_to_kernel(as);
		#endif

		/* The address space should no longer be in use. */
		assert(refcount_get(&as->count) == 0);
	}

	/* Unmap and destroy each region. */
	LIST_FOREACH_SAFE(&as->regions, iter)
		vm_region_destroy(list_entry(iter, vm_region_t, header));

	/* Destroy the MMU context. */
	mmu_context_destroy(as->mmu);

	assert(list_empty(&as->regions));
	assert(avl_tree_empty(&as->tree));

	slab_cache_free(vm_aspace_cache, as);
}

/** Show information about a region within an address space.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_region(int argc, char **argv, kdb_filter_t *filter) {
	uint64_t val;
	process_t *process;
	vm_aspace_t *as;
	vm_region_t *region;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <process ID|addr> <addr>\n\n", argv[0]);

		kdb_printf("Prints details about the region containing the given address in the address\n");
		kdb_printf("space specified by the first argument.\n");
		return KDB_SUCCESS;
	} else if(argc != 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(kdb_parse_expression(argv[1], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	if(val >= KERNEL_BASE) {
		as = (vm_aspace_t *)((ptr_t)val);
	} else {
		process = process_lookup_unsafe(val);
		if(!process) {
			kdb_printf("Invalid process ID.\n");
			return KDB_FAILURE;
		}

		as = process->aspace;
	}

	if(kdb_parse_expression(argv[2], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	region = vm_region_find(as, val, true);
	if(!region) {
		kdb_printf("Region not found.\n");
		return KDB_FAILURE;
	}

	kdb_printf("Region %p (%s)\n", region, (region->name) ? region->name : "<unnamed>");
	kdb_printf("=================================================\n");
	kdb_printf("as:          %p\n", region->as);
	kdb_printf("start:       %p\n", region->start);
	kdb_printf("size:        0x%zx\n", region->size);
	kdb_printf("protection:  0x%" PRIx32 "\n", region->protection);
	kdb_printf("flags:       0x%" PRIx32 "\n", region->flags);
	switch(region->state) {
	case VM_REGION_FREE:
		kdb_printf("state:       %d (free)\n", region->state);
		break;
	case VM_REGION_ALLOCATED:
		kdb_printf("state:       %d (allocated)\n", region->state);
		break;
	case VM_REGION_RESERVED:
		kdb_printf("state:       %d (reserved)\n", region->state);
		break;
	default:
		kdb_printf("state:       %d (invalid)\n", region->state);
		break;
	}
	kdb_printf("handle:      %p\n", region->handle);
	kdb_printf("obj_offset:  0x%" PRIx64 "\n", region->obj_offset);
	kdb_printf("amap:        %p\n", region->amap);
	kdb_printf("amap_offset: 0x%" PRIx64 "\n", region->amap_offset);

	return KDB_SUCCESS;
}

/** Display details of a region.
 * @param region	Region to display. */
static void dump_region(vm_region_t *region) {
	kdb_printf("%-18p 0x%-12zx 0x%-5" PRIx32 " 0x%-3" PRIx32 " %-5d 0x%-8" PRIx64
		" %s\n", region->start, region->size, region->protection,
		region->flags, region->state, region->obj_offset,
		(region->name) ? region->name : "<unnamed>");
}

/** Dump an address space.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_aspace(int argc, char **argv, kdb_filter_t *filter) {
	enum { DUMP_ALL, DUMP_ALLOCATED, DUMP_FREE } mode;
	uint64_t val;
	process_t *process;
	vm_aspace_t *as;
	unsigned i;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s <process ID|addr>\n", argv[0]);
		kdb_printf("       %s --allocated <process ID|addr>\n", argv[0]);
		kdb_printf("       %s --free <process ID|addr>\n\n", argv[0]);

		kdb_printf("The first form prints some details about an address space and a list of all\n");
		kdb_printf("regions (free, reserved and allocated) from the sorted region list. The second\n");
		kdb_printf("form prints the content of the allocated region tree. The final form prints the\n");
		kdb_printf("content of the address space's free lists.\n");
		return KDB_SUCCESS;
	} else if(argc < 2 || argc > 3) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	mode = DUMP_ALL;

	if(argc > 2) {
		if(strcmp(argv[1], "--free") == 0) {
			mode = DUMP_FREE;
		} else if(strcmp(argv[1], "--allocated") == 0) {
			mode = DUMP_ALLOCATED;
		} else {
			kdb_printf("Unrecognized option. See 'help %s' for help.\n", argv[0]);
			return KDB_FAILURE;
		}
	}

	if(kdb_parse_expression(argv[argc - 1], &val, NULL) != KDB_SUCCESS)
		return KDB_FAILURE;

	if(val >= KERNEL_BASE) {
		as = (vm_aspace_t *)((ptr_t)val);
	} else {
		process = process_lookup_unsafe(val);
		if(!process) {
			kdb_printf("Invalid process ID.\n");
			return KDB_FAILURE;
		}

		as = process->aspace;
	}

	if(mode == DUMP_ALL) {
		if(process) {
			kdb_printf("Aspace %p\n", as);
		} else {
			kdb_printf("Aspace %p (%s)\n", as, process->name);
		}
		kdb_printf("=================================================\n");

		kdb_printf("lock:       %d (%" PRId32 ")\n", atomic_get(&as->lock.value),
			(as->lock.holder) ? as->lock.holder->id : -1);
		kdb_printf("count:      %d\n", refcount_get(&as->count));
		kdb_printf("find_cache: %p\n", as->find_cache);
		kdb_printf("mmu:        %p\n", as->mmu);
		kdb_printf("free_map:   0x%lx\n\n", as->free_map);
	}

	if(mode == DUMP_FREE) kdb_printf("List ");
	kdb_printf("%-18s %-14s %-7s %-5s %-5s %-10s %s\n",
		"Start", "Size", "Protect", "Flags", "State", "Offset", "Name");
	if(mode == DUMP_FREE) kdb_printf("==== ");
	kdb_printf("%-18s %-14s %-7s %-5s %-5s %-10s %s\n",
		"=====", "====", "=======", "=====", "=====", "======", "====");

	switch(mode) {
	case DUMP_ALL:
		LIST_FOREACH(&as->regions, iter)
			dump_region(list_entry(iter, vm_region_t, header));

		break;
	case DUMP_ALLOCATED:
		AVL_TREE_FOREACH(&as->tree, iter)
			dump_region(avl_tree_entry(iter, vm_region_t, tree_link));

		break;
	case DUMP_FREE:
		for(i = 0; i < VM_FREELISTS; i++) {
			LIST_FOREACH(&as->free[i], iter) {
				kdb_printf("%-4u ", i);
				dump_region(list_entry(iter, vm_region_t, free_link));
			}
		}

		break;
	}

	return KDB_SUCCESS;
}

/** Initialize the VM system. */
__init_text void vm_init(void) {
	/* Create the VM slab caches. */
	vm_aspace_cache = slab_cache_create("vm_aspace_cache", sizeof(vm_aspace_t),
		0, vm_aspace_ctor, NULL, NULL, 0, MM_BOOT);
	vm_region_cache = slab_cache_create("vm_region_cache", sizeof(vm_region_t),
		0, vm_region_ctor, NULL, NULL, 0, MM_BOOT);
	vm_amap_cache = slab_cache_create("vm_amap_cache", sizeof(vm_amap_t),
		0, vm_amap_ctor, NULL, NULL, 0, MM_BOOT);

	/* Bring up the page daemons. */
	page_daemon_init();

	/* Initialize the caching system. */
	vm_cache_init();

	/* Register the KDB commands. */
	kdb_register_command("region", "Print details about a VM region.",
		kdb_cmd_region);
	kdb_register_command("aspace", "Print details about an address space.",
		kdb_cmd_aspace);
}

/**
 * Map an object into memory.
 *
 * Creates a new memory mapping that maps either an object or anonymous memory.
 * The spec argument controls where the mapping will be placed. The following
 * address specifications are currently defined:
 *
 *  - VM_ADDRESS_ANY: The mapping can be placed anywhere available in the
 *    address space, an unused space will be allocated to fit it in.
 *  - VM_ADDRESS_EXACT: The mapping will be placed at exactly the address
 *    specified, and any existing mappings in the same region will be replaced.
 *
 * The flags argument controls the behaviour of the mapping. The following flags
 * are currently defined:
 *
 *  - VM_MAP_PRIVATE: Modifications to the mapping will not be transferred
 *    through to the source object, and if the address space is duplicated, the
 *    duplicate and original will be given copy-on-write copies of the region.
 *    If this flag is not specified and the address space is duplicated, changes
 *    made in either address space will be visible in the other.
 *  - VM_MAP_OVERCOMMIT: Memory will not be reserved for the mapping, meaning
 *    it can be made larger than the total memory available (memory is only
 *    allocated when it is actually accessed). The default behaviour is to only
 *    allow mappings if the memory requirement can be satisfied.
 *  - VM_MAP_INHERIT: When a child process is created via kern_process_create()
 *    or the current process is replaced via kern_process_replace(), the
 *    mapping will be duplicated into the new address space, using the semantics
 *    specified above for VM_MAP_PRIVATE. This can be used to pass data to
 *    child processes.
 *
 * When mapping an object, the calling process must have the correct access
 * rights to the object for the mapping permissions requested.
 *
 * @param addrp		For VM_ADDRESS_ANY, points to a variable in which to
 *			store the allocated address. For VM_ADDRESS_EXACT,
 *			points to a variable containing the address to place
 *			the mapping at/
 * @param size		Size of mapping (multiple of page size).
 * @param spec		Address specification (VM_ADDRESS_*).
 * @param protection	Memory protection flags (VM_PROT_*).
 * @param flags		Mapping behaviour flags (VM_MAP_*).
 * @param handle	Handle to object to map in. If NULL, then the region
 *			will be an anonymous memory mapping.
 * @param offset	Offset into object to map from (multiple of page size).
 * @param name		Name of the memory mapping, for informational purposes.
 *			Can be NULL.
 *
 * @return		Status code describing result of the operation.
 */
status_t kern_vm_map(void **addrp, size_t size, unsigned spec, uint32_t protection,
	uint32_t flags, handle_t handle, offset_t offset, const char *name)
{
	object_handle_t *khandle = NULL;
	ptr_t addr;
	char *kname = NULL;
	status_t ret;

	if(!addrp)
		return STATUS_INVALID_ARG;

	ret = memcpy_from_user(&addr, addrp, sizeof(addr));
	if(ret != STATUS_SUCCESS)
		return ret;

	if(name) {
		ret = strndup_from_user(name, REGION_NAME_MAX, &kname);
		if(ret != STATUS_SUCCESS)
			return ret;
	}

	if(handle >= 0) {
		ret = object_handle_lookup(handle, -1, 0, &khandle);
		if(ret != STATUS_SUCCESS) {
			kfree(kname);
			return ret;
		}
	}

	ret = vm_map(curr_proc->aspace, &addr, size, spec, protection, flags,
		khandle, offset, kname);

	if(ret == STATUS_SUCCESS) {
		ret = memcpy_to_user(addrp, &addr, sizeof(addr));
		if(ret != STATUS_SUCCESS)
			vm_unmap(curr_proc->aspace, addr, size);
	}

	if(khandle)
		object_handle_release(khandle);

	kfree(kname);
	return ret;
}

/**
 * Unmaps a region of memory.
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
