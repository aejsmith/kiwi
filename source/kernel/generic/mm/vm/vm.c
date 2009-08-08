/* Kiwi virtual memory manager
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
 * @brief		Virtual memory manager.
 *
 * The virtual memory manager facilitates the creation and manipulation of
 * per-process address spaces. It allows files, devices and regions of
 * anonymous memory to be mapped into these address spaces. It also handles
 * movement of pages out of memory to disk if the system is low on memory.
 *
 * Parts of the design are inspired by NetBSD's UVM (although not the same as),
 * in particular the implementation of anonymous memory and copy-on-write.
 *
 * Reference:
 * - The UVM Virtual Memory System.
 *   http://citeseerx.ist.psu.edu/viewdoc/summary?doi=10.1.1.28.1236
 */

#include <arch/memmap.h>

#include <cpu/intr.h>

#include <io/vfs.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/safe.h>
#include <mm/slab.h>
#include <mm/tlb.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <assert.h>
#include <errors.h>
#include <kdbg.h>

#include "vm_priv.h"

#if CONFIG_VM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Slab caches used for VM structures. */
static slab_cache_t *vm_aspace_cache;	/**< Cache of address space structures. */
static slab_cache_t *vm_region_cache;	/**< Cache of region structures. */

/** Constructor for address space objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int vm_aspace_ctor(void *obj, void *data, int kmflag) {
	vm_aspace_t *as = (vm_aspace_t *)obj;

	mutex_init(&as->lock, "vm_aspace_lock", 0);
	refcount_set(&as->count, 0);
	avl_tree_init(&as->regions);
	return 0;
}

/** Allocate a new region structure. Caller must attach object to it.
 * @param as		Address space of the region.
 * @param start		Start address of the region.
 * @param end		End address of the region.
 * @param flags		Flags for the region.
 * @return		Pointer to region structure. */
static vm_region_t *vm_region_alloc(vm_aspace_t *as, ptr_t start, ptr_t end, int flags) {
	vm_region_t *region = slab_cache_alloc(vm_region_cache, MM_SLEEP);

	list_init(&region->object_link);
	region->as = as;
	region->start = start;
	region->end = end;
	region->flags = flags;
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
	avl_tree_node_t *node;
	return (node = avl_tree_node_next(region->node)) ? avl_tree_entry(node, vm_region_t) : NULL;
}

/** Unmap all or part of a region.
 * @note		This function is called whenever part of a region is
 *			going to be removed. It unmaps pages covering the area,
 *			and then calls the unmap operation for the region's
 *			object, if any.
 * @note		If the entire region is being unmapped, the caller must
 *			detach the region from the object.
 * @param region	Region being unmapped (should not be reserved).
 * @param start		Start of range to unmap.
 * @param end		End of range to unmap. */
static void vm_region_unmap(vm_region_t *region, ptr_t start, ptr_t end) {
	phys_ptr_t paddr;
	offset_t offset;
	ptr_t i;

	assert(!(region->flags & VM_REGION_RESERVED));

	for(i = start; i < end; i += PAGE_SIZE) {
		if(page_map_remove(&region->as->pmap, i, &paddr) != 0) {
			continue;
		}

		/* Release the page just unmapped. */
		offset = (offset_t)(i - region->start) + region->offset;
		if(region->object->ops->page_release) {
			region->object->ops->page_release(region->object, offset, paddr);
		}
	}

	/* Invalidate the TLB entries on all CPUs using the address space. */
	tlb_invalidate(region->as, start, end);

	/* Tell the object that we've done this, if necessary. */
	if(region->object->ops->unmap) {
		region->object->ops->unmap(region->object, region->offset + (start - region->start), end - start);
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

	/* If not reserved, unmap pages in the areas we're not going to cover
	 * any more, and let the object know that we're doing this. */
	if(!(region->flags & VM_REGION_RESERVED)) {
		if(region->end - end) {
			vm_region_unmap(region, end, region->end);
		}
		if(start - region->start) {
			vm_region_unmap(region, region->start, start);
			region->offset += (start - region->start);
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

		/* Point the split at the object and reference it. */
		split->object = region->object;
		split->offset = region->offset + (start - region->start);
		split->object->ops->get(split->object, split);
	}

	/* Change the size of the old region. */
	region->end = end;

	/* Insert the split region. */
	avl_tree_insert(&split->as->regions, (key_t)split->start, split, &split->node);
}

/** Unmap an entire region.
 * @param region	Region to destroy. */
static void vm_region_destroy(vm_region_t *region) {
	/* Unmap all pages and let the object know we've unmapped this, and
	 * drop our reference to the object. */
	if(!(region->flags & VM_REGION_RESERVED)) {
		vm_region_unmap(region, region->start, region->end);
		region->object->ops->release(region->object, region);
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
			if((ASPACE_BASE + size) <= region->start) {
				*addrp = ASPACE_BASE;
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
		if((prev->end + size) > prev->end && (prev->end + size) <= (ASPACE_BASE + ASPACE_SIZE)) {
			/* We have some space, return it. */
			*addrp = prev->end;
			return true;
		}
	} else if(size <= ASPACE_SIZE) {
		*addrp = ASPACE_BASE;
		return true;
	}

	return false;
}

/** Check mapping arguments.
 * @param start		Start address.
 * @param size		Size of mapping.
 * @param flags		Mapping behaviour flags.
 * @param offset	Offset into object.
 * @param addrp		Location to store allocated address in.
 * @return		0 if arguments OK, negative error code on failure. */
static int vm_map_check_args(ptr_t start, size_t size, int flags, offset_t offset, ptr_t *addrp) {
	if(!size || size % PAGE_SIZE || offset % PAGE_SIZE) {
		return -ERR_PARAM_INVAL;
	} else if(flags & VM_MAP_FIXED) {
		if(start % PAGE_SIZE || !vm_region_fits(start, size)) {
			return -ERR_PARAM_INVAL;
		}
	} else if(!addrp) {
		return -ERR_PARAM_INVAL;
	}

	return 0;
}

/** Perform the actual work of mapping a region.
 * @param as		Address space to map into (should be locked).
 * @param start		Start address to map at (if not VM_MAP_FIXED).
 * @param size		Size of the region to map.
 * @param flags		Mapping behaviour flags.
 * @param object	Object that the region should map.
 * @param offset	Offset into the object the region should map from.
 * @param addrp		Where to store allocated address (if not VM_MAP_FIXED).
 * @return		0 on success, negative error code on failure. */
static int vm_map_internal(vm_aspace_t *as, ptr_t start, size_t size, int flags,
                           vm_object_t *object, offset_t offset, ptr_t *addrp) {
	vm_region_t *region;
	int ret, rflags;

	assert(!vm_map_check_args(start, size, flags, offset, addrp));
	assert(object);

	/* Convert mapping flags to region flags. */
	rflags = flags & (VM_MAP_READ | VM_MAP_WRITE | VM_MAP_EXEC | VM_MAP_PRIVATE);

	/* If allocating space, we must now find some. Otherwise, we free up
	 * anything in the location we want to insert to. */
	if(!(flags & VM_MAP_FIXED)) {
		if(!vm_find_free(as, size, &start)) {
			return -ERR_NO_MEMORY;
		}

		*addrp = start;
	} else {
		vm_unmap_internal(as, start, start + size);
	}

	/* Create the region structure, and attach the object to it. */
	region = vm_region_alloc(as, start, start + size, rflags);
	region->object = object;
	region->offset = offset;
	object->ops->get(object, region);

	/* Tell the object we're mapping it in. */
	if(object->ops->map) {
		if((ret = object->ops->map(object, offset, size)) != 0) {
			object->ops->release(object, region);
			slab_cache_free(vm_region_cache, region);
			return ret;
		}
	}

	/* Insert the region into the tree. */
	avl_tree_insert(&as->regions, (key_t)region->start, region, &region->node);

	dprintf("vm: mapped region [%p,%p) (as: %p, obj: %p, flags(m/r): %d/%d)\n",
	        region->start, region->end, as, object, flags, rflags);
	return 0;
}

/** Initialize a VM object structure.
 *
 * Initializes a VM object structure and sets it to use the specified object
 * operations structure.
 *
 * @param object	Object to initialize.
 * @param ops		Operations to use for the object.
 */
void vm_object_init(vm_object_t *object, vm_object_ops_t *ops) {
	/* Check operations structure. */
	assert(ops->get);
	assert(ops->release);
	assert(ops->fault || ops->page_get);

	object->ops = ops;
}

/** Destroy a VM object structure.
 *
 * Destroys a VM object structure. Note that this function currently does
 * nothing. It is defined in case it is needed in the future.
 *
 * @param object	Object to destroy.
 */
void vm_object_destroy(vm_object_t *object) {
	/* Nothing happens. */
}

/** Page fault handler.
 *
 * Attempts to handle a page fault within an address space. If the object for
 * the region the fault occurred on has its own fault handler, it is called.
 * Otherwise, the generic fault handler uses the page_get operation of the
 * region to get a page and map it.
 *
 * @param addr		Address the fault occurred at.
 * @param reason	Reason for the fault.
 * @param access	Type of memory access that caused the fault.
 *
 * @return		VM_FAULT_HANDLED or VM_FAULT_UNHANDLED.
 */
int vm_fault(ptr_t addr, int reason, int access) {
	vm_aspace_t *as = curr_aspace;
	vm_region_t *region;
	vm_page_t *page;
	offset_t offset;
	int ret, flags;

	/* If we don't have an address space, don't do anything. */
	if(!as) {
		return VM_FAULT_UNHANDLED;
	}

	dprintf("vm: page fault at %p (as: %p, reason: %d, access: %d)\n", addr, as, reason, access);

	/* Round down address to a page boundary. */
	addr &= PAGE_MASK;

	/* Safe to take the lock despite us being in an interrupt - the lock
	 * is only held within the functions in this file, and they should not
	 * incur a pagefault (if they do there's something wrong!). */
	mutex_lock(&as->lock, 0);

	/* Find the region that the fault occurred in - if its a reserved
	 * region, the memory is unmapped so treat it as though no region is
	 * there. */
	if(unlikely(!(region = vm_region_find(as, addr, NULL)))) {
		goto fault;
	} else if(unlikely(region->flags & VM_REGION_RESERVED)) {
		goto fault;
	}

	assert(region->object);
	assert(region->object->ops->fault || region->object->ops->page_get);

	/* Check whether the access is allowed. */
	if((access == VM_FAULT_READ && !(region->flags & VM_REGION_READ)) ||
	   (access == VM_FAULT_WRITE && !(region->flags & VM_REGION_WRITE)) ||
	   (access == VM_FAULT_EXEC && !(region->flags & VM_REGION_EXEC))) {
		goto fault;
	}

	/* Pass the fault through to the object's handler if it has one. */
	if(region->object->ops->fault) {
		ret = region->object->ops->fault(region, addr, reason, access);
		mutex_unlock(&as->lock);
		return ret;
	}

	/* Get a page from the object. */
	offset = (offset_t)(addr - region->start) + region->offset;
	ret = region->object->ops->page_get(region->object, offset, &page);
	if(unlikely(ret != 0)) {
		dprintf("vm:  failed to get page for %p (%d)\n", addr, ret);
		goto fault;
	}

	/* Protection faults must be write faults. We check protection flags
	 * above, and the only protection fault we intentionally cause is a
	 * write one. */
	if(reason == VM_FAULT_PROTECTION) {
		if(access != VM_MAP_WRITE) {
			fatal("Non-write protection fault at %p", addr);
		}

		/* Unmap previous entry. */
		if(unlikely(page_map_remove(&as->pmap, addr, NULL) != 0)) {
			fatal("Could not remove previous mapping for %p", addr);
		}

		/* Invalidate the TLB entries. */
		tlb_invalidate(as, addr, addr);
	}

	/* Work out the flags to map with. If we're not writing, and the page
	 * is not already dirty, mark it as read-only, so we can make the page
	 * dirty when it gets written to. */
	flags = vm_region_flags_to_page(region->flags);
	if(access != VM_MAP_WRITE) {
		if(!(page->flags & VM_PAGE_DIRTY)) {
			dprintf("vm:  page 0x%" PRIpp " not dirty yet, mapping read-only\n", page->addr);
			flags &= ~PAGE_MAP_WRITE;
		}
	} else {
		page->flags |= VM_PAGE_DIRTY;
		dprintf("vm:  flagged page 0x%" PRIpp " as dirty\n", page->addr);
	}

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	if(unlikely(page_map_insert(&as->pmap, addr, page->addr, flags, MM_SLEEP) != 0)) {
		fatal("Failed to insert page map entry for %p", addr);
	}

	dprintf("vm:  mapped 0x%" PRIpp " at %p (as: %p, flags: %d)\n", page->addr, addr, as, flags);
	mutex_unlock(&as->lock);
	return VM_FAULT_HANDLED;
fault:
	mutex_unlock(&as->lock);
	return VM_FAULT_UNHANDLED;
}

#if 0
# pragma mark Public interface.
#endif

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

	mutex_lock(&as->lock, 0);

	/* Allocate the region structure. */
	region = vm_region_alloc(as, start, start + size, VM_REGION_RESERVED);
	region->object = NULL;
	region->offset = 0;

	/* Create a hole and insert it into the address space. */
	vm_unmap_internal(as, start, start + size);
	avl_tree_insert(&as->regions, (key_t)region->start, region, &region->node);
	mutex_unlock(&as->lock);
	return 0;
}

/** Map a region of anonymous memory.
 *
 * Maps a region of anonymous memory (i.e. not backed by any data source) into
 * an address space. If the VM_MAP_FIXED flag is specified, then the region
 * will be mapped at the exact location specified, and any existing mappings in
 * the same region will be overwritten. Otherwise, a region of unused space
 * will be allocated for the mapping. If the VM_MAP_PRIVATE flag is specified,
 * then the region will not be shared if the address space is duplicated - the
 * duplicate and the original address space will be given copy-on-write copies
 * of the region. If the VM_MAP_PRIVATE flag is not specified and the address
 * space is duplicated, changes made in the original address space and the new
 * address space will be visible in the other.
 *
 * @param as		Address space to map in.
 * @param start		Start address of region (if VM_MAP_FIXED).
 * @param size		Size of region to map (multiple of system page size).
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_map_anon(vm_aspace_t *as, ptr_t start, size_t size, int flags, ptr_t *addrp) {
	vm_object_t *object;
	int ret;

	if((ret = vm_map_check_args(start, size, flags, 0, addrp)) != 0) {
		return ret;
	}

	/* Create a new anonymous object. */
	object = vm_anon_object_create(size, NULL, 0);

	mutex_lock(&as->lock, 0);

	/* Attempt to map the region in. */
	if((ret = vm_map_internal(as, start, size, flags, object, 0, addrp)) != 0) {
		vm_anon_object_destroy(object);
	}

	mutex_unlock(&as->lock);
	return ret;
}

/** Map a file into memory.
 *
 * Maps all or part of a file into the calling process' address space. If the
 * VM_MAP_FIXED flag is specified, then the region will be mapped at the exact
 * location specified, and any existing mappings in the same region will be
 * overwritten. Otherwise, a region of unused space will be allocated for the
 * mapping. If the VM_MAP_PRIVATE flag is specified, then a copy-on-write
 * mapping will be created - changes to the mapped data will not be made in the
 * underlying file, and will not be visible to other regions mapping the file.
 * Also, changes made to the file's data after the mapping has been written
 * to may not be visible in the mapping. If the process duplicates itself,
 * changes made in the child after the duplication will not be visible in the
 * parent, and vice-versa. If the VM_MAP_PRIVATE flag is not specified, then
 * changes to the mapped data will be made in the underlying file, and will be
 * visible to other regions mapping the file.
 *
 * @param as		Address space to map in.
 * @param start		Start address of region (if VM_MAP_FIXED).
 * @param size		Size of region to map (multiple of system page size).
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param node		Node to map in (will have an extra reference on if
 *			function succeeds).
 * @param offset	Offset into file to map from.
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int vm_map_file(vm_aspace_t *as, ptr_t start, size_t size, int flags, vfs_node_t *node, offset_t offset, ptr_t *addrp) {
	vm_object_t *object;
	int ret;

	if((ret = vm_map_check_args(start, size, flags, offset, addrp)) != 0) {
		return ret;
	} else if(node->type != VFS_NODE_FILE) {
		return -ERR_TYPE_INVAL;
	}

	/* If this is a private mapping, we must create an anonymous object
	 * on top of the file. */
	if(flags & VM_MAP_PRIVATE) {
		object = vm_anon_object_create(size, &node->vobj, offset);
		offset = 0;
	} else {
		object = &node->vobj;
	}

	mutex_lock(&as->lock, 0);

	/* Attempt to map the region in. */
	if((ret = vm_map_internal(as, start, size, flags, object, offset, addrp)) != 0) {
		if(flags & VM_MAP_PRIVATE) {
			vm_anon_object_destroy(object);
		}
	}

	mutex_unlock(&as->lock);
	return ret;
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

	mutex_lock(&as->lock, 0);
	vm_unmap_internal(as, start, start + size);
	mutex_unlock(&as->lock);

	dprintf("vm: unmapped region [%p,%p) (as: %p)\n", start, start + size, as);
	return 0;
}

/** Switch to another address space.
 *
 * Switches to a different address space. Does not take address space lock
 * because this function is used during rescheduling.
 *
 * @param as		Address space to switch to (if NULL, then will switch
 *			to the kernel address space).
 */
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
 *
 * Allocates a new address space structure and initializes it.
 *
 * @return		Pointer to address space structure, NULL on failure.
 */
vm_aspace_t *vm_aspace_create(void) {
	vm_aspace_t *as;

	as = slab_cache_alloc(vm_aspace_cache, MM_SLEEP);
	if(page_map_init(&as->pmap) != 0) {
		slab_cache_free(vm_aspace_cache, as);
		return NULL;
	}

	as->find_cache = NULL;

	/* Do architecture-specific initialization. */
	if(vm_aspace_arch_init(as) != 0) {
		page_map_destroy(&as->pmap);
		slab_cache_free(vm_aspace_cache, as);
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

/** Initialize the address space caches. */
void __init_text vm_init(void) {
	vm_aspace_cache = slab_cache_create("vm_aspace_cache", sizeof(vm_aspace_t),
	                                    0, vm_aspace_ctor, NULL, NULL, NULL,
	                                    NULL, 0, MM_FATAL);
	vm_region_cache = slab_cache_create("vm_region_cache", sizeof(vm_region_t),
	                                    0, NULL, NULL, NULL, NULL, NULL, 0,
	                                    MM_FATAL);

	/* Initialize other parts of the VM system. */
	vm_anon_init();
	vm_page_init();
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
		} else if(!(process = process_lookup(val))) {
			kprintf(LOG_NONE, "Invalid process ID.\n");
			return KDBG_FAIL;
		}

		as = process->aspace;
	}

	kprintf(LOG_NONE, "Base               End                Flags Object             Offset\n");
	kprintf(LOG_NONE, "====               ===                ===== ======             ======\n");

	AVL_TREE_FOREACH(&as->regions, iter) {
		region = avl_tree_entry(iter, vm_region_t);

		kprintf(LOG_NONE, "%-18p %-18p %-5d %-18p %" PRId64 "\n",
		        region->start, region->end, region->flags,
		        region->object, region->offset);
	}

	return KDBG_OK;
}

#if 0
# pragma mark System calls.
#endif

/** Map a region of anonymous memory.
 *
 * Maps a region of anonymous memory (i.e. not backed by any data source) into
 * the calling process' address space. If the VM_MAP_FIXED flag is specified,
 * then the region will be mapped at the exact location specified, and any
 * existing mappings in the same region will be overwritten. Otherwise, a
 * region of unused space will be allocated for the mapping. If the
 * VM_MAP_PRIVATE flag is specified, then the region will not be shared if the
 * process duplicates itself - the child and the original process will be
 * given copy-on-write copies of the region. If the VM_MAP_PRIVATE flag is not
 * specified and the process duplicates itself, changes made by the parent and
 * the child will be visible to each other.
 *
 * @param start		Start address of region (if VM_MAP_FIXED).
 * @param size		Size of region to map (multiple of system page size).
 * @param flags		Flags to control mapping behaviour (VM_MAP_*).
 * @param addrp		Where to store address of mapping.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_vm_map_anon(void *start, size_t size, int flags, void **addrp) {
	ptr_t addr;
	int ret;

	if((ret = vm_map_anon(curr_proc->aspace, (ptr_t)start, size, flags, &addr)) != 0) {
		return ret;
	}

	/* TODO: Better write functions for integer values. */
	return memcpy_to_user(addrp, &addr, sizeof(void *));
}

/** Map a file into memory.
 *
 * Maps all or part of a file into the calling process' address space. If the
 * VM_MAP_FIXED flag is specified, then the region will be mapped at the exact
 * location specified, and any existing mappings in the same region will be
 * overwritten. Otherwise, a region of unused space will be allocated for the
 * mapping. If the VM_MAP_PRIVATE flag is specified, then a copy-on-write
 * mapping will be created - changes to the mapped data will not be made in the
 * underlying file, and will not be visible to other regions mapping the file.
 * Also, changes made to the file's data after the mapping has been written
 * to may not be visible in the mapping. If the process duplicates itself,
 * changes made in the child after the duplication will not be visible in the
 * parent, and vice-versa. If the VM_MAP_PRIVATE flag is not specified, then
 * changes to the mapped data will be made in the underlying file, and will be
 * visible to other regions mapping the file.
 *
 * @param args		Pointer to arguments structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int sys_vm_map_file(vm_map_file_args_t *args) {
	return -ERR_NOT_IMPLEMENTED;
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
