/* Kiwi anonymous VM object management
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
 * @brief		Anonymous VM object management.
 *
 * A brief note about reference counting for pages:
 *  - The reference count in the page structure is used to track how many
 *    anonymous objects refer to a single page (i.e. object has been duplicated
 *    but the page has not been copied, because no write fault has occurred).
 *    If, when a write fault occurs on a page, the page structure reference
 *    count is greater than 1, the page is copied. Otherwise, the page is just
 *    remapped as read-write (if the region is VM_REGION_WRITE, that is).
 *  - Each object also contains an array of reference counts (obj->rref - the
 *    region reference count) for each page that the object can cover. This
 *    array is used to track how many regions are mapping each page of the
 *    object, allowing pages to be freed when no more regions refer to them.
 *
 * @todo		The page array could be changed into a two-level array,
 *			which would reduce memory consumption for large,
 *			sparsely-used objects.
 *
 * @note		This code does not bother marking pages as dirty. It
 *			has no need to do so. It also does not use the offset
 *			field in the page structure, because pages can be
 *			shared between multiple anonymous objects at different
 *			offsets in each.
 */

#include <mm/malloc.h>
#include <mm/slab.h>
#include <mm/tlb.h>

#include <assert.h>
#include <fatal.h>

#include "vm_priv.h"

/** Structure describing an anonymous VM object. */
typedef struct vm_anon_object {
	vm_object_t header;		/**< Object header. */

	refcount_t count;		/**< Count of regions referring to this object. */
	mutex_t lock;			/**< Lock to protect object. */
	list_t regions;			/**< List of regions mapping the object. */

	size_t curr_size;		/**< Number of pages currently contained in object. */
	size_t max_size;		/**< Maximum number of pages in object. */
	vm_page_t **pages;		/**< Array of pages currently in object. */
	uint16_t *rref;			/**< Region reference count array. */

	vm_object_t *source;		/**< Source object to fetch missing pages from. */
	offset_t offset;		/**< Offset into the source object. */
} vm_anon_object_t;

/** Slab cache for allocating anonymous objects. */
static slab_cache_t *vm_anon_object_cache;

/** Increase the reference count of an anonymous object.
 * @param _obj		Object to increase count of.
 * @param region	Region that the reference is for. */
static void vm_anon_object_get(vm_object_t *_obj, vm_region_t *region) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;

	/* Objects with sources should only be attached to private regions. */
	if(obj->source && !(region->flags & VM_REGION_PRIVATE)) {
		fatal("Non-private region referencing anonymous object with source");
	}

	mutex_lock(&obj->lock, 0);
	refcount_inc(&obj->count);
	list_append(&obj->regions, &region->object_link);
	mutex_unlock(&obj->lock);
}

/** Decrease the reference count of an anonymous object.
 * @param _obj		Object to decrease count of.
 * @param region	Region being detached. */
static void vm_anon_object_release(vm_object_t *_obj, vm_region_t *region) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;

	mutex_lock(&obj->lock, 0);

	/* Detach the region from the object. */
	list_remove(&region->object_link);

	if(refcount_dec(&obj->count) > 0) {
		mutex_unlock(&obj->lock);
		return;
	}

	mutex_unlock(&obj->lock);
	vm_anon_object_destroy(_obj);
}

/** Map part of an anonymous object.
 * @param _obj		Object being mapped.
 * @param offset	Offset into the object that the region starts at.
 * @param size		Size of the region being created.
 * @return		0 on success, negative error code on failure. */
static int vm_anon_object_map(vm_object_t *_obj, offset_t offset, size_t size) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;
	size_t i, start, end;

	mutex_lock(&obj->lock, 0);

	/* Work out the entries within the object that this covers and ensure
	 * it's within the object - for anonymous objects mappings can't be
	 * outside the object. */
	start = (size_t)(offset >> PAGE_WIDTH);
	end = start + (size >> PAGE_WIDTH);
	assert(end <= obj->max_size);

	/* Increase the region reference counts for pages in the region. */
	for(i = start; i < end; i++) {
		if(obj->rref[i] == UINT16_MAX) {
			/* TODO: Should probably handle this properly, although
			 * it seems unlikely that the object will be shared
			 * between more than 65,535 regions. */
			fatal("Object %p rref[%zu] is at maximum value!", obj, i);
		}
		obj->rref[i]++;
	}

	mutex_unlock(&obj->lock);
	return 0;
}

/** Unmap part of an anonymous object.
 * @param _obj		Object being unmapped.
 * @param offset	Offset into object being unmapped.
 * @param size		Size of area being unmapped. */
static void vm_anon_object_unmap(vm_object_t *_obj, offset_t offset, size_t size) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;
	size_t i, start, end;

	mutex_lock(&obj->lock, 0);

	/* Work out the entries within the object that this covers and ensure
	 * it's within the object. */
	start = (size_t)(offset >> PAGE_WIDTH);
	end = start + (size >> PAGE_WIDTH);
	assert(end <= obj->max_size);

	/* Decrease the region reference counts for the pages in the region,
	 * and free allocated pages if we do not need them any more. */
	for(i = start; i < end; i++) {
		assert(obj->rref[i]);

		if(--obj->rref[i] == 0 && obj->pages[i]) {
			dprintf("vm: anon object rref count %zu reached 0, freeing 0x%" PRIpp " (obj: %p)\n",
			        i, obj->pages[i]->addr, obj);
			if(refcount_dec(&obj->pages[i]->count) == 0) {
				vm_page_free(obj->pages[i]);
			}
			obj->pages[i] = NULL;
			obj->curr_size--;
		}
	}

	mutex_unlock(&obj->lock);
}

/** Copy a private region using an anonymous object.
 * @param src		Source region to copy from.
 * @param dest		Destination region to copy to.
 * @return		0 on success, negative error code on failure. */
static int vm_anon_object_copy(vm_region_t *src, vm_region_t *dest) {
	vm_anon_object_t *srcobj = (vm_anon_object_t *)src->object, *destobj;
	size_t i, start, end;
	int ret;

	assert(src->flags & VM_REGION_PRIVATE);

	mutex_lock(&srcobj->lock, 0);

	/* Work out the entries within the source object that the destination
	 * region covers. */
	start = (size_t)(src->offset >> PAGE_WIDTH);
	end = start + (size_t)((src->end - src->start) >> PAGE_WIDTH);
	assert(end <= srcobj->max_size);

	/* Allocate an object for the new region. */
	destobj = slab_cache_alloc(vm_anon_object_cache, MM_SLEEP);
	destobj->curr_size = 0;
	destobj->max_size = end - start;
	destobj->pages = kcalloc(destobj->max_size, sizeof(vm_page_t *), MM_SLEEP);
	destobj->rref = kcalloc(destobj->max_size, sizeof(uint16_t *), MM_SLEEP);
	if(srcobj->source) {
		destobj->source = srcobj->source;
		destobj->offset = srcobj->offset + src->offset;
		destobj->source->ops->get(destobj->source, NULL);
	} else {
		destobj->source = NULL;
		destobj->offset = 0;
	}

	refcount_set(&destobj->count, 1);

	/* Point all of the pages in the destination to the pages from the
	 * source: they will be copied when a write fault occurs on either the
	 * source or the destination. Set the region reference count for each
	 * page to 1, to account for the destination region. */
	for(i = start; i < end; i++) {
		if(srcobj->pages[i]) {
			refcount_inc(&srcobj->pages[i]->count);
		}
		destobj->pages[i - start] = srcobj->pages[i];
		destobj->rref[i - start] = 1;
	}

	/* Point the destination at the object. */
	list_append(&destobj->regions, &dest->object_link);
	dest->object = &destobj->header;
	dest->offset = 0;

	/* Write-protect all mappings on the source region. Should not fail,
	 * we use MM_SLEEP, and page_map_protect() is supposed to ignore
	 * missing entries. */
	ret = page_map_protect(&src->as->pmap, src->start, src->end,
	                       vm_region_flags_to_page(src->flags & ~VM_REGION_WRITE));
	if(ret != 0) {
		fatal("Could not write-protect original region (%d)", ret);
	}

	/* Invalidate TLB entries for the range in the source address space. */
	tlb_invalidate(src->as, src->start, src->end);

	dprintf("vm: copied anonymous region %p (obj: %p) to %p (obj: %p)\n",
	        src, src->object, dest, dest->object);
	mutex_unlock(&srcobj->lock);
	return 0;
}

/** Handle a fault on an anonymous region.
 * @param region	Region fault occurred in.
 * @param addr		Virtual address of fault (rounded down to base of page).
 * @param reason	Reason for the fault.
 * @param access	Type of access that caused the fault.
 * @return		Fault status code. */
static int vm_anon_object_fault(vm_region_t *region, ptr_t addr, int reason, int access) {
	vm_anon_object_t *obj = (vm_anon_object_t *)region->object;
	phys_ptr_t paddr;
	offset_t offset;
	vm_page_t *page;
	int ret, flags;
	size_t i;

	/* Work out the offset into the object. */
	offset = region->offset + (addr - region->start);
	i = (size_t)(offset >> PAGE_WIDTH);

	mutex_lock(&obj->lock, 0);

	assert(i < obj->max_size);

	/* Do some sanity checks if this is a protection fault. The main fault
	 * handler verifies that the access is allowed by the region flags, so
	 * the only access type protection faults should be is write. COW
	 * faults should never occur on non-private regions, either. */
	if(reason == VM_FAULT_PROTECTION) {
		if(unlikely(access != VM_FAULT_WRITE)) {
			fatal("Non-write protection fault at %p on %p", addr, obj);
		} else if(unlikely(!(region->flags & VM_REGION_PRIVATE))) {
			fatal("Copy-on-write fault at %p on non-private region", addr);
		}
	}

	/* Get the page and work out the flags to map with. */
	flags = vm_region_flags_to_page(region->flags);
	if(!obj->pages[i] && !obj->source) {
		/* No page existing and no source. Allocate a zeroed page. */
		dprintf("vm:  anon fault: no existing page and no source, allocating new\n");
		obj->pages[i] = vm_page_alloc(MM_SLEEP | PM_ZERO);
		obj->curr_size++;
		paddr = obj->pages[i]->addr;
	} else if(access == VM_MAP_WRITE) {
		if(obj->pages[i]) {
			assert(refcount_get(&obj->pages[i]->count) > 0);

			/* If the reference count is greater than 1 we must
			 * copy it. Shared regions should not contain any pages
			 * with a reference count greater than 1. */
			if(refcount_get(&obj->pages[i]->count) > 1) {
				assert(region->flags & VM_REGION_PRIVATE);

				dprintf("vm:  anon write fault: copying page %zu due to refcount > 1\n", i);

				page = vm_page_copy(obj->pages[i], MM_SLEEP);

				/* Decrease the count of the old page. We must
				 * handle it going to 0 here, as another object
				 * could have released the page while we were
				 * copying. */
				if(refcount_dec(&obj->pages[i]->count) == 0) {
					vm_page_free(obj->pages[i]);
				}

				obj->pages[i] = page;
			}
			
			paddr = obj->pages[i]->addr;
		} else {
			assert(region->flags & VM_REGION_PRIVATE);

			/* Find the page to copy. If handling a protection
			 * fault, use the existing mapping address. */
			if(reason == VM_FAULT_PROTECTION) {
				if(unlikely(!page_map_find(&region->as->pmap, addr, &paddr))) {
					fatal("No mapping for %p, but protection fault on it", addr);
				}
			} else {
				ret = obj->source->ops->page_get(obj->source, offset + obj->offset, &page);
				if(unlikely(ret != 0)) {
					dprintf("vm:  could not read page from anonymous object source (%d)\n", ret);
					mutex_unlock(&obj->lock);
					return VM_FAULT_UNHANDLED;
				}
				paddr = page->addr;
			}

			dprintf("vm:  anon write fault: copying page 0x%" PRIpp " from %p\n",
			        paddr, obj->source);

			page = vm_page_alloc(MM_SLEEP);
			page_copy(page->addr, paddr, MM_SLEEP);

			/* Add the page and release the old one. */
			obj->pages[i] = page;
			if(obj->source->ops->page_release) {
				obj->source->ops->page_release(obj->source, offset + obj->offset, paddr);
			}

			obj->curr_size++;
			paddr = page->addr;
		}
	} else {
		if(obj->pages[i]) {
			assert(refcount_get(&obj->pages[i]->count) > 0);

			/* If the reference count is greater than 1, map read
			 * only. */
			if(refcount_get(&obj->pages[i]->count) > 1) {
				assert(region->flags & VM_REGION_PRIVATE);
				flags &= ~PAGE_MAP_WRITE;
			}

			paddr = obj->pages[i]->addr;
		} else {
			assert(region->flags & VM_REGION_PRIVATE);

			/* Get the page from the source, and map read-only. */
			ret = obj->source->ops->page_get(obj->source, offset + obj->offset, &page);
			if(unlikely(ret != 0)) {
				dprintf("vm:  could not read page from anonymous object source (%d)\n", ret);
				mutex_unlock(&obj->lock);
				return VM_FAULT_UNHANDLED;
			}

			paddr = page->addr;

			dprintf("vm:  anon read fault: mapping page 0x%" PRIpp " from %p as read-only\n",
			        paddr, obj->source);

			flags &= ~PAGE_MAP_WRITE;
		}
	}

	/* The page address should now be stored in paddr, and flags should be
	 * set correctly. If this is a protection fault, remove existing
	 * mappings. */
	if(reason == VM_FAULT_PROTECTION) {
		if(unlikely(page_map_remove(&region->as->pmap, addr, NULL) != 0)) {
			fatal("Could not remove previous mapping for %p", addr);
		}

		/* Invalidate the TLB entries. */
		tlb_invalidate(region->as, addr, addr);
	}

	/* Map the entry in. Should always succeed with MM_SLEEP set. */
	if(unlikely(page_map_insert(&region->as->pmap, addr, paddr, flags, MM_SLEEP) != 0)) {
		fatal("Failed to insert page map entry for %p", addr);
	}

	dprintf("vm:  anon fault: mapped 0x%" PRIpp " at %p (as: %p, flags: %d)\n",
	        paddr, addr, region->as, flags);
	mutex_unlock(&obj->lock);
	return VM_FAULT_HANDLED;
}

/** Release a page from an anonymous object.
 * @note		This function is needed to ensure that pages that have
 *			been mapped from the source object and never replaced
 *			with an anonymous page get released.
 * @param _obj		Object to release page in.
 * @param offset	Offset of page in object.
 * @param paddr		Physical address of page that was unmapped. */
static void vm_anon_object_page_release(vm_object_t *_obj, offset_t offset, phys_ptr_t paddr) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;
	size_t i = (size_t)(offset >> PAGE_WIDTH);

	assert(i < obj->max_size);

	/* If page is in the object, then do nothing. */
	if(obj->pages[i]) {
		assert(obj->pages[i]->addr == paddr);
		return;
	}

	/* Page must have come from the source. Release it there. */
	assert(obj->source);
	assert(obj->source->ops->page_release);

	obj->source->ops->page_release(obj->source, offset + obj->offset, paddr);
}

/** Anonymous object operations. */
static vm_object_ops_t vm_anon_object_ops = {
	.get = vm_anon_object_get,
	.release = vm_anon_object_release,
	.map = vm_anon_object_map,
	.unmap = vm_anon_object_unmap,
	.copy = vm_anon_object_copy,
	.fault = vm_anon_object_fault,
	.page_release = vm_anon_object_page_release,
};

/** Constructor for anonymous VM objects.
 * @param _obj		Object to construct.
 * @param data		Data pointer (unused).
 * @param kmflag	Allocation flags.
 * @return		Always returns 0. */
static int vm_anon_object_ctor(void *_obj, void *data, int kmflag) {
	vm_anon_object_t *obj = _obj;

	vm_object_init(&obj->header, &vm_anon_object_ops);
	refcount_set(&obj->count, 0);
	mutex_init(&obj->lock, "vm_anon_object_lock", 0);
	list_init(&obj->regions);
	return 0;
}

/** Create an anonymous VM object.
 *
 * Creates a new anonymous VM object structure, optionally with a backing
 * source. If provided, the backing source will be used to get pages that are
 * not found in the object, and they will be copied into it. Otherwise, the
 * object will be a zero-filled object.
 *
 * @param size		Size of object.
 * @param source	Optional backing object (will be referenced).
 * @param offset	Offset into backing object.
 *
 * @return		Pointer to created object (will have a 0 reference
 *			count - get must be called on it when attaching it to
 *			a region).
 */
vm_object_t *vm_anon_object_create(size_t size, vm_object_t *source, offset_t offset) {
	vm_anon_object_t *obj;

	/* An anonymous object should not be created over another anonymous
	 * object, or an object requiring special fault handling. */
	if(source) {
		assert(source->ops != &vm_anon_object_ops);
		assert(!source->ops->fault);
	}
	assert(size);

	obj = slab_cache_alloc(vm_anon_object_cache, MM_SLEEP);
	obj->curr_size = 0;
	obj->max_size = size >> PAGE_WIDTH;
	obj->pages = kcalloc(obj->max_size, sizeof(vm_page_t *), MM_SLEEP);
	obj->rref = kcalloc(obj->max_size, sizeof(uint16_t *), MM_SLEEP);
	obj->source = source;
	obj->offset = offset;

	/* Reference the source, if any. We currently give it a NULL region
	 * pointer. This is probably not good. */
	if(source) {
		source->ops->get(source, NULL);
	}

	dprintf("vm: created anonymous object %p (size: %zu, pages: %zu, source: %p, offset: %" PRId64 ")\n",
	        obj, size, obj->max_size, source, offset);
	return &obj->header;
}

/** Destroy an anonymous object.
 *
 * Destroys an anonymous object structure. Reference count must be 0.
 *
 * @param _obj		Object to destroy.
 */
void vm_anon_object_destroy(vm_object_t *_obj) {
	vm_anon_object_t *obj = (vm_anon_object_t *)_obj;

	assert(refcount_get(&obj->count) == 0);
	assert(!obj->curr_size);

	/* Release the source object. Pass a NULL region pointer because it was
	 * referenced with no region. */
	if(obj->source) {
		obj->source->ops->release(obj->source, NULL);
	}

	dprintf("vm: destroyed anonymous object %p (source: %p)\n", obj, obj->source);
	kfree(obj->rref);
	kfree(obj->pages);
	slab_cache_free(vm_anon_object_cache, obj);
}

/** Initialize the anonymous object cache. */
void __init_text vm_anon_init(void) {
	vm_anon_object_cache = slab_cache_create("vm_anon_object_cache", sizeof(vm_anon_object_t),
	                                         0, vm_anon_object_ctor, NULL, NULL,
	                                         NULL, NULL, 0, MM_FATAL);
}
