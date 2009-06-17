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

#if CONFIG_PMM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Array of boot-reclaimable ranges. */
static struct { phys_ptr_t start; phys_ptr_t end; } pmm_reclaim_ranges[64] __init_data;
static size_t pmm_reclaim_count __init_data = 0;

/** Vmem arena used for page allocations. */
static vmem_t pmm_arena;

/** Zero a range of pages.
 * @param base		Base address to zero from.
 * @param size		Number of pages to zero.
 * @param pmflag	Allocation flags.
 * @return		True on success, false on failure. */
static bool pmm_zero_range(phys_ptr_t base, size_t count, int pmflag) {
	void *mapping;

	mapping = page_phys_map(base, (count * PAGE_SIZE), (pmflag & MM_FLAG_MASK) & ~MM_FATAL);
	if(mapping == NULL) {
		if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %" PRIs " pages (2)", count);
		}
		return false;
	}

	memset(mapping, 0, (count * PAGE_SIZE));
	page_phys_unmap(mapping, (count * PAGE_SIZE));
	return true;
}

/** Allocate a range of pages with constraints.
 *
 * Allocates a range of pages. Flags can be specified to modify the allocation
 * behaviour, and constraints can be specified to control where the allocation
 * is made. Allocations made with this function should only be freed with
 * pmm_xfree().
 *
 * @param count		Number of pages to allocate.
 * @param align		Alignment of allocation.
 * @param phase		Offset from alignment boundary.
 * @param nocross	Alignment boundary the allocation should not go across.
 * @param minaddr	Minimum start address of the allocation.
 * @param maxaddr	Highest end address of the allocation.
 * @param pmflag	Flags to control allocation behaviour.
 *
 * @return		Base address of range allocated or 0 if unable to
 *			allocate.
 */
phys_ptr_t pmm_xalloc(size_t count, phys_ptr_t align, phys_ptr_t phase,
                      phys_ptr_t nocross, phys_ptr_t minaddr,
                      phys_ptr_t maxaddr, int pmflag) {
	size_t size = (count * PAGE_SIZE);
	phys_ptr_t base;

	/* First allocate the range from Vmem and try to reclaim from Slab
	 * if unable to allocate. */
	while(!(base = (phys_ptr_t)vmem_xalloc(&pmm_arena, size, (vmem_resource_t)align,
	                                       (vmem_resource_t)phase, (vmem_resource_t)nocross,
	                                       (vmem_resource_t)minaddr, (vmem_resource_t)maxaddr,
                                               (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
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
		if(!pmm_zero_range(base, count, pmflag)) {
			vmem_xfree(&pmm_arena, (vmem_resource_t)base, size);
			return 0;
		}
	}

	dprintf("pmm: allocated page range [0x%" PRIpp ",0x%" PRIpp ") (constrained)\n",
		base, base + size);
	return base;
}

/** Free a range of pages.
 *
 * Frees a range of pages. Parameters passed to this function must exactly
 * match those of the original allocation, i.e. you cannot allocate a range
 * of 6 pages then try to only free 4 of them. Only use this function if the
 * original allocation was made with pmm_xalloc().
 *
 * @param base		Base address of page range.
 * @param count		Number of pages to free.
 */
void pmm_xfree(phys_ptr_t base, size_t count) {
	vmem_xfree(&pmm_arena, (vmem_resource_t)base, (count * PAGE_SIZE));

	dprintf("pmm: freed page range [0x%" PRIpp ",0x%" PRIpp ") (constrained)\n",
		base, base + (count * PAGE_SIZE));
	return;
}

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
	phys_ptr_t base;

	/* First allocate the range from Vmem and try to reclaim from Slab
	 * if unable to allocate. */
	while(!(base = (phys_ptr_t)vmem_alloc(&pmm_arena, size, (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
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
		if(!pmm_zero_range(base, count, pmflag)) {
			vmem_free(&pmm_arena, (vmem_resource_t)base, size);
			return 0;
		}
	}

	dprintf("pmm: allocated page range [0x%" PRIpp ",0x%" PRIpp ")\n",
		base, base + size);
	return base;
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
	phys_ptr_t ret;

	/* Mark the pages covering the range as in-use. */
	ret = vmem_xalloc(&pmm_arena, (vmem_resource_t)(end - start), 0, 0, 0,
	                  (vmem_resource_t)start, (vmem_resource_t)end, 0);
	if(ret != start) {
		fatal("Couldn't mark [0x%" PRIpp ", 0x%" PRIpp ") as reclaimable (%" PRIpp ")",
		      start, end);
	}

	/* Record the reclaimable region. */
	if(pmm_reclaim_count > ARRAYSZ(pmm_reclaim_ranges)) {
		fatal("Out of reclaim range structures");
	}
	pmm_reclaim_ranges[pmm_reclaim_count].start = start;
	pmm_reclaim_ranges[pmm_reclaim_count].end = end;
	pmm_reclaim_count++;
}

/** Mark part of a page range as in-use.
 *
 * Marks part of an existing page range as in-use.
 *
 * @param start		Start of range.
 * @param end		End of range.
 */
void pmm_mark_reserved(phys_ptr_t start, phys_ptr_t end) {
	phys_ptr_t ret;

	/* Mark the pages covering the range as in-use. */
	ret = vmem_xalloc(&pmm_arena, (size_t)(end - start), 0, 0, 0,
	                  (vmem_resource_t)start, (vmem_resource_t)end, 0);
	if(ret != start) {
		fatal("Could not mark region [0x%" PRIpp ", 0x%" PRIpp ") as reserved",
		      start, end);
	}
}

/** Initialize the physical memory manager. */
void pmm_init(void) {
	vmem_early_create(&pmm_arena, "pmm_arena", 0, 0, PAGE_SIZE, NULL, NULL, NULL, 0, MM_FATAL);

	/* Populate the arena with memory regions. This function is
	 * implemented by the architecture or platform. */
	pmm_populate();
}

/** Reclaim memory no longer in use after kernel initialization.
 * @note		It is OK for this function to clear regions despite
 *			the reclaim information structures being there because
 *			nothing should make any allocations while this is
 *			running. */
void pmm_init_reclaim(void) {
	size_t reclaimed = 0, size, i;

	for(i = 0; i < pmm_reclaim_count; i++) {
		size = (size_t)(pmm_reclaim_ranges[i].end - pmm_reclaim_ranges[i].start);

		vmem_xfree(&pmm_arena, (vmem_resource_t)pmm_reclaim_ranges[i].start, size);
		reclaimed += size;
	}

	kprintf(LOG_DEBUG, "pmm: reclaimed %" PRIs " KiB unused kernel memory\n", (reclaimed / 1024));
}
