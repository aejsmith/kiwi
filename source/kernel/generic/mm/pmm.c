/* Kiwi physical memory manager
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
 * @brief		Physical memory manager.
 */

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>
#include <mm/pmm.h>
#include <mm/slab.h>
#include <mm/vmem.h>

#include <assert.h>
#include <fatal.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void pmm_arch_init(void *data);

/** Vmem arena used for page allocations. */
static vmem_t pmm_arena;

/** Allocate a range of pages.
 *
 * Allocates a range of pages. Flags can be specified to modify the allocation
 * behaviour.
 *
 * @param count		Number of pages to allocate.
 * @param pmflag	Flags to control allocation behaviour.
 *
 * @return		Base address of range allocated or 0 if unable to
 *			allocate.
 */
phys_ptr_t pmm_alloc(size_t count, int pmflag) {
	size_t size = (count * PAGE_SIZE);
	phys_ptr_t page;
	void *addr;

	/* First allocate the range from Vmem and try to reclaim from Slab
	 * if unable to allocate. */
	while(!(page = (phys_ptr_t)vmem_alloc(&pmm_arena, size, (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
		if(slab_reclaim()) {
			continue;
		} else if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %" PRIs " pages (1)", count);
		} else {
			return 0;
		}
	}

	/* Handle zeroing requests. */
	if(pmflag & PM_ZERO) {
		addr = page_phys_map(page, size, (pmflag & MM_FLAG_MASK) & ~MM_FATAL);
		if(addr == NULL) {
			if(pmflag & MM_FATAL) {
				fatal("Could not perform mandatory allocation of %" PRIs " pages (2)", count);
			}
			vmem_free(&pmm_arena, (vmem_resource_t)page, size);
			return 0;
		}

		memset(addr, 0, size);
		page_phys_unmap(addr, size);
	}

	dprintf("pmm: allocated page range [0x%" PRIpp ",0x%" PRIpp ")\n",
		page, page + size);
	return page;
}

/** Free a range of pages.
 *
 * Frees a range of pages. Parameters passed to this function must exactly
 * match those of the original allocation, i.e. you cannot allocate a range
 * of 6 pages then try to only free 4 of them.
 *
 * @param base		Base address of page range.
 * @param count		Number of pages to free.
 */
void pmm_free(phys_ptr_t base, size_t count) {
	vmem_free(&pmm_arena, (vmem_resource_t)base, (count * PAGE_SIZE));

	dprintf("pmm: freed page range [0x%" PRIpp ",0x%" PRIpp ")\n",
		base, base + (count * PAGE_SIZE));
	return;
}

/** Add a range of free pages to the page arena.
 *
 * Adds a range of free pages to the page allocator's Vmem arena. This range
 * must not overlap an existing range.
 *
 * @param start		Start of range.
 * @param end		End of range.
 */
void pmm_add(phys_ptr_t start, phys_ptr_t end) {
	vmem_add(&pmm_arena, (vmem_resource_t)start, (vmem_resource_t)(end - start), MM_FATAL);
}

/** Mark part of a page range as temporarily in-use.
 *
 * Marks part of an existing page range as temporarily in-use, to be freed
 * when pmm_init_reclaim() is called.
 *
 * @param start		Start of range.
 * @param end		End of range.
 */
void pmm_mark_reclaimable(phys_ptr_t start, phys_ptr_t end) {
	// TODO
}

/** Mark part of a page range as in-use.
 *
 * Marks part of an existing page range as in-use.
 *
 * @param start		Start of range.
 * @param end		End of range.
 */
void pmm_mark_reserved(phys_ptr_t start, phys_ptr_t size) {
	// TODO
}

/** Initialize the physical memory manager. */
void pmm_init(void *data) {
	vmem_early_create(&pmm_arena, "pmm_arena", 0, 0, PAGE_SIZE, NULL, NULL, NULL, 0, MM_FATAL);

	/* Get the architecture to do any initialization it requires and add
	 * in the memory regions that we can use. */
	pmm_arch_init(data);
}

/** Reclaim memory no longer needed after kernel initialization. */
void pmm_init_reclaim(void) {
#if 0
	size_t reclaimed = 0, i;
	boot_region_t *bregion;
	page_region_t *region;

	/* Quick note here: its safe to free the pages despite the fact that
	 * the temporary heap for the region structures is in there - no
	 * allocations should take place while this function is running. */

	LIST_FOREACH_SAFE(&boot_regions, iter) {
		bregion = list_entry(iter, boot_region_t, header);
		list_remove(&bregion->header);

		assert(bregion->flags & PAGE_REGION_RECLAIM);

		region = page_region_find(bregion->start);
		if(region == NULL) {
			fatal("Could not find regions while reclaiming");
		}
		spinlock_unlock(&region->lock);

		/* Mark all the pages as free. */
		for(i = 0; i < region->page_count; i++) {
			page_free(&region->pages[i]);
		}

		reclaimed += (bregion->end - bregion->start);
	}

	kprintf(LOG_DEBUG, "page: reclaimed %" PRIs " KiB unused kernel memory\n", (reclaimed / 1024));
#endif
}
