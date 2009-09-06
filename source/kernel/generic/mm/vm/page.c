/* Kiwi VM page management
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
 * @brief		VM page management.
 *
 * The functions in this file provide a higher level system on top of the
 * physical memory manager for tracking pages being used within the VM system.
 * Each page allocated by it is assigned a structure which can be used to
 * store information such as how many regions are using a page, whether a page
 * is dirty, etc. Having a structure associated with pages also makes it
 * easier to make lists/trees of pages.
 *
 * @todo		Zero pages that are free but still cached by slab
 *			periodically (or when the system is idle) so that
 *			allocations of pages with PM_ZERO set are faster.
 */

#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <assert.h>

#include "vm_priv.h"

/** Slab cache for page structures. */
static slab_cache_t *vm_page_cache;

/** Constructor for page objects.
 * @param obj		Pointer to object.
 * @param data		Ignored.
 * @param kmflag	Allocation flags.
 * @return		0 on success, -1 on failure. */
static int vm_page_ctor(void *obj, void *data, int kmflag) {
	vm_page_t *page = obj;

	list_init(&page->header);
	refcount_set(&page->count, 0);
	page->offset = 0;
	page->flags = 0;

	/* Cache page allocations. Thanks to the magic of slab reclaiming,
	 * this won't starve the kernel itself of pages! */
	if(!(page->addr = page_alloc(1, kmflag & MM_FLAG_MASK))) {
		return -1;
	}

	return 0;
}

/** Destructor for page objects.
 * @param obj		Pointer to object.
 * @param data		Ignored. */
static void vm_page_dtor(void *obj, void *data) {
	vm_page_t *page = obj;

	page_free(page->addr, 1);
}

/** Copy a VM page.
 *
 * Allocates a new page and copies the contents of the specified page to it,
 * and returns a pointer to the new page's structure. The new page does not
 * inherit anything (flags, etc) from the old page, other than the data within
 * the page.
 *
 * @param page		Structure for page to copy.
 * @param mmflag	Allocation flags.
 *
 * @return		Pointer to page structure on success, NULL on failure.
 */
vm_page_t *vm_page_copy(vm_page_t *page, int mmflag) {
	vm_page_t *copy;

	/* Clear out anything we don't want, such as PM_ZERO. */
	mmflag &= MM_FLAG_MASK;

	if(!(copy = slab_cache_alloc(vm_page_cache, mmflag))) {
		return NULL;
	} else if(page_copy(copy->addr, page->addr, mmflag) != 0) {
		slab_cache_free(vm_page_cache, copy);
		return NULL;
	}

	refcount_inc(&copy->count);
	return copy;
}

/** Allocate a VM page.
 *
 * Allocates a page and a structure for it that can be used by the VM system.
 * If specified, the page will be zeroed. The returned page will have one
 * reference on it.
 *
 * @param pmflag	Allocation flags.
 *
 * @return		Pointer to page structure on success, NULL on failure.
 */
vm_page_t *vm_page_alloc(int pmflag) {
	vm_page_t *page;

	if(!(page = slab_cache_alloc(vm_page_cache, pmflag & MM_FLAG_MASK))) {
		return NULL;
	}

	/* Zero the page if required. */
	if(pmflag & PM_ZERO) {
		if(page_zero(page->addr, pmflag & MM_FLAG_MASK) != 0) {
			slab_cache_free(vm_page_cache, page);
			return NULL;
		}
	}

	refcount_inc(&page->count);
	return page;
}

/** Free a VM page.
 *
 * Free the page described by a VM page structure. Page reference count should
 * be 0.
 *
 * @param page		Page to free.
 */
void vm_page_free(vm_page_t *page) {
	assert(refcount_get(&page->count) == 0);

	slab_cache_free(vm_page_cache, page);
}

/** Initialise the VM page allocator. */
void __init_text vm_page_init(void) {
	vm_page_cache = slab_cache_create("vm_page_cache", sizeof(vm_page_t),
	                                  0, vm_page_ctor, vm_page_dtor, NULL,
	                                  NULL, NULL, 0, MM_FATAL);
}
