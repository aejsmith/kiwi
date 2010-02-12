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
 * @brief		Physical memory management.
 */

#include <arch/memmap.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vmem.h>

#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kargs.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern char __init_start[], __init_end[];

/** Array of boot-reclaimable ranges. */
static struct { phys_ptr_t start; phys_ptr_t end; } g_reclaim_ranges[64] __init_data;
static size_t g_reclaim_range_count __init_data = 0;

/** Vmem arena used for page allocations. */
static vmem_t g_page_arena;

/** Zero a range of pages.
 * @param base		Base address to zero from.
 * @param size		Size of range to zero.
 * @param pmflag	Allocation flags.
 * @return		True on success, false on failure. */
static bool page_range_zero(phys_ptr_t base, size_t size, int pmflag) {
	void *mapping;

	thread_wire(curr_thread);

	mapping = page_phys_map(base, size, (pmflag & MM_FLAG_MASK) & ~MM_FATAL);
	if(mapping == NULL) {
		if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %zu pages (2)", size / PAGE_SIZE);
		}
		thread_unwire(curr_thread);
		return false;
	}

	memset(mapping, 0, size);
	page_phys_unmap(mapping, size, false);
	thread_unwire(curr_thread);
	return true;
}

/** Allocate a range of pages with constraints.
 *
 * Allocates a range of pages. Flags can be specified to modify the allocation
 * behaviour, and constraints can be specified to control where the allocation
 * is made. Allocations made with this function should only be freed with
 * page_xfree().
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
phys_ptr_t page_xalloc(size_t count, phys_ptr_t align, phys_ptr_t phase,
                       phys_ptr_t nocross, phys_ptr_t minaddr,
                       phys_ptr_t maxaddr, int pmflag) {
	size_t size = (count * PAGE_SIZE);
	phys_ptr_t base;

	if(!(base = vmem_xalloc(&g_page_arena, size, align, phase, nocross, minaddr,
	                        maxaddr, (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
		if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %zu pages (1)", count);
		}
		return 0;
	}

	/* Handle zeroing requests. */
	if(pmflag & PM_ZERO) {
		if(!page_range_zero(base, size, pmflag)) {
			vmem_xfree(&g_page_arena, base, size);
			return 0;
		}
	}

	dprintf("page: allocated page range [0x%" PRIpp ",0x%" PRIpp ") (constrained)\n",
		base, base + size);
	return base;
}

/** Free a range of pages.
 *
 * Frees a range of pages. Parameters passed to this function must exactly
 * match those of the original allocation, i.e. you cannot allocate a range
 * of 6 pages then try to only free 4 of them. Only use this function if the
 * original allocation was made with page_xalloc().
 *
 * @param base		Base address of page range.
 * @param count		Number of pages to free.
 */
void page_xfree(phys_ptr_t base, size_t count) {
	vmem_xfree(&g_page_arena, base, (count * PAGE_SIZE));

	dprintf("page: freed page range [0x%" PRIpp ",0x%" PRIpp ") (constrained)\n",
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
phys_ptr_t page_alloc(size_t count, int pmflag) {
	size_t size = (count * PAGE_SIZE);
	phys_ptr_t base;

	if(!(base = vmem_alloc(&g_page_arena, size, (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
		if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %zu pages (1)", count);
		}
		return 0;
	}

	/* Handle zeroing requests. */
	if(pmflag & PM_ZERO) {
		if(!page_range_zero(base, size, pmflag)) {
			vmem_free(&g_page_arena, base, size);
			return 0;
		}
	}

	dprintf("page: allocated page range [0x%" PRIpp ",0x%" PRIpp ")\n",
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
void page_free(phys_ptr_t base, size_t count) {
	vmem_free(&g_page_arena, (vmem_resource_t)base, (count * PAGE_SIZE));

	dprintf("page: freed page range [0x%" PRIpp ",0x%" PRIpp ")\n",
		base, base + (count * PAGE_SIZE));
	return;
}

/** Zero the contents of a page.
 *
 * Zeroes the contents of the specified page. The allocation flags argument is
 * used to specify behaviour when mapping the page into memory (this does not
 * apply on architectures such as AMD64 where pages are always mapped in).
 *
 * @param addr		Physical address of page to zero.
 * @param mmflag	Allocation flags for mapping page in memory.
 *
 * @return		0 on success, negative error code on failure.
 */
int page_zero(phys_ptr_t addr, int mmflag) {
	void *mapping;

	thread_wire(curr_thread);

	if(!(mapping = page_phys_map(addr, PAGE_SIZE, mmflag))) {
		thread_unwire(curr_thread);
		return -ERR_NO_MEMORY;
	}

	memset(mapping, 0, PAGE_SIZE);
	page_phys_unmap(mapping, PAGE_SIZE, false);
	thread_unwire(curr_thread);
	return 0;
}

/** Copy the contents of a page.
 *
 * Copies the contents of one page to another. The allocation flags argument is
 * used to specify behaviour when mapping the pages into memory (this does not
 * apply on architectures such as AMD64 where pages are always mapped in).
 *
 * @param dest		Destination page.
 * @param dest		Source page.
 * @param mmflag	Allocation flags for mapping page in memory.
 *
 * @return		0 on success, negative error code on failure.
 */
int page_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag) {
	void *mdest, *msrc;

	thread_wire(curr_thread);

	if(!(mdest = page_phys_map(dest, PAGE_SIZE, mmflag))) {
		thread_unwire(curr_thread);
		return -ERR_NO_MEMORY;
	} else if(!(msrc = page_phys_map(source, PAGE_SIZE, mmflag))) {
		thread_unwire(curr_thread);
		page_phys_unmap(mdest, PAGE_SIZE, false);
		return -ERR_NO_MEMORY;
	}

	memcpy(mdest, msrc, PAGE_SIZE);
	page_phys_unmap(msrc, PAGE_SIZE, false);
	page_phys_unmap(mdest, PAGE_SIZE, false);
	thread_unwire(curr_thread);
	return 0;
}

/** Mark a range of pages as reclaimable.
 * @param start		Start of range.
 * @param end		End of range. */
static void __init_text page_mark_reclaimable(phys_ptr_t start, phys_ptr_t end) {
	/* Allocate the range so nothing else allocates from it. */
	vmem_xalloc(&g_page_arena, end - start, 0, 0, 0, start, end, MM_FATAL);

	/* Record details of it so it can be reclaimed later. */
	if(g_reclaim_range_count >= ARRAYSZ(g_reclaim_ranges)) {
		fatal("Out of reclaim range structures");
	}
	g_reclaim_ranges[g_reclaim_range_count].start = start;
	g_reclaim_ranges[g_reclaim_range_count].end = end;
	g_reclaim_range_count++;
}

/** Initialise the physical memory manager.
 * @param args		Kernel arguments. */
void __init_text page_init(kernel_args_t *args) {
	kernel_args_memory_t *range;
	phys_ptr_t start, end;
	uint32_t i;

	vmem_early_create(&g_page_arena, "page_arena", 0, 0, PAGE_SIZE, NULL, NULL,
	                  NULL, 0, VMEM_RECLAIM, MM_FATAL);

	/* Populate the arena with physical memory ranges given by the
	 * bootloader. */
	for(i = 0; i < args->phys_range_count; i++) {
		range = &args->phys_ranges[i];
		switch(range->type) {
		case PHYS_MEMORY_FREE:
			vmem_add(&g_page_arena, range->start, range->end - range->start, MM_FATAL);
			break;
		case PHYS_MEMORY_RECLAIMABLE:
			vmem_add(&g_page_arena, range->start, range->end - range->start, MM_FATAL);
			page_mark_reclaimable(range->start, range->end);
			break;
		default:
			/* Don't care about anything else. */
			break;
		}
	}

	/* Mark the kernel init section as reclaimable. Since the kernel is
	 * marked as allocated by the bootloader, the range will not have
	 * been added by the above loop. Therefore, the range must be added
	 * first. */
	start = ((ptr_t)__init_start - KERNEL_VIRT_BASE) + args->kernel_phys;
	end = ((ptr_t)__init_end - KERNEL_VIRT_BASE) + args->kernel_phys;
	vmem_add(&g_page_arena, start, end - start, MM_FATAL);
	page_mark_reclaimable(start, end);

	/* Initialise architecture paging-related things. */
	page_arch_init(args);
}

/** Reclaim memory no longer in use after kernel initialisation. */
void __init_text page_late_init(void) {
	size_t reclaimed = 0, size, i;

	page_arch_late_init();

	/* It is OK to clear regions despite the reclaim information structures
	 * being in a reclaimable region because nothing should make any
	 * allocations while this is running. */
	for(i = 0; i < g_reclaim_range_count; i++) {
		size = g_reclaim_ranges[i].end - g_reclaim_ranges[i].start;
		vmem_xfree(&g_page_arena, g_reclaim_ranges[i].start, size);
		reclaimed += size;
	}

	kprintf(LOG_NORMAL, "page: reclaimed %zu KiB of unneeded memory\n", (reclaimed / 1024));
}
