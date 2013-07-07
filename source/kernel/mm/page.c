/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		Physical memory management.
 *
 * This file contains everything related to the management of physical memory.
 * We keep track of physical memory with the page_t structure: each page of
 * usable memory has one of these representing it, that tracks the state of the
 * page and how it is being used.
 *
 * Each usable memory range in the range list provided by the loader is stored
 * in a global array and has an array of page_t's allocated for it. This global
 * array is called the page database, and is used to quickly look up the
 * structure associated with a physical address.
 *
 * There are a number of queues that a page can be placed in depending on its
 * state, and these queues are used for various purposes. Below is a
 * description of each queue:
 *  - Allocated: Pages which have been allocated and are currently in-use.
 *  - Modified:  Pages which have been modified and need to be written to their
 *               source. There is a special thread (the page writer) that
 *               periodically takes pages off this queue and writes them. This
 *               functionality is used by the cache system to ensure that
 *               modifications to data get written to the source soon, rather
 *               than staying in memory for a long time without being written.
 *  - Cached:    Pages that are not currently mapped, but are holding cached
 *               data. Pages are taken from this queue and freed up when the
 *               number of free pages gets low.
 * The movement of pages between queues as required is mostly left up to the
 * users of the pages: pages will just be placed on the allocated queue when
 * first allocated, and must be moved manually using page_set_state().
 *
 * Free pages are stored in a number of lists. Allocating a single page is just
 * a matter of popping a page from the first list that has free pages. The
 * lists are separated in a platform-specific manner. This is done to improve
 * allocation speed with commonly used minimum/maximum address constraints. For
 * example, the PC platform separates the lists into below 16MB (ISA DMA),
 * below 4GB (devices that use 32-bit DMA addresses) and anything else, since
 * these are the most likely constraints that will be used. Allocations using
 * these constraints can be satisfied simply by popping a page from an
 * appropriate list.
 *
 * The allocation code is not optimised for quick allocations of contiguous
 * ranges of pages, or pages with alignment/boundary constraints, as it is
 * assumed that this will not be done frequently (e.g. in driver initialization
 * routines). The method used to do this is to search through the entire page
 * database to find free pages that satisfy the constraints.
 *
 * Locking rules:
 *  - Free page lock must be held to set a page's state to PAGE_STATE_FREE, or
 *    to change it away from PAGE_STATE_FREE.
 *  - Free page lock and a page queue lock cannot be held at the same time.
 *
 * @todo		Pre-zero free pages while idle.
 * @todo		Reservations of pages for allocations from userspace.
 *			When swap is implemented, the count of memory available
 *			to reserve will include swap space. This means that
 *			allocation will not overcommit memory.
 */

#include <arch/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/vm_cache.h>

#include <proc/thread.h>

#include <sync/mutex.h>

#include <assert.h>
#include <kboot.h>
#include <kdb.h>
#include <kernel.h>
#include <status.h>
#include <time.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Boot memory range structure. */
typedef struct boot_range {
	phys_ptr_t start;		/**< Start of the range. */
	phys_size_t size;		/**< Total size of the range. */
	phys_size_t allocated;		/**< Space allocated in the range. */
	unsigned freelist;		/**< Free page list index. */
} boot_range_t;

/** Structure describing a range of physical memory. */
typedef struct memory_range {
	phys_ptr_t start;		/**< Start of the range. */
	phys_ptr_t end;			/**< End of the range. */
	page_t *pages;			/**< Pages in the range. */
	unsigned freelist;		/**< Free page list index. */
} memory_range_t;

/** Structure containing a page queue. */
typedef struct page_queue {
	list_t pages;			/**< List of pages. */
	page_num_t count;		/**< Number of pages in the queue. */
	spinlock_t lock;		/**< Lock to protect the queue. */
} page_queue_t;

/** Structure containing a free page list. */
typedef struct page_freelist {
	list_t pages;			/**< Pages in the list. */
	phys_ptr_t minaddr;		/**< Lowest start address contained in the list. */
	phys_ptr_t maxaddr;		/**< Highest end address contained in the list. */
} page_freelist_t;

/** Page writer settings. */
#define PAGE_WRITER_INTERVAL		SECS2NSECS(4)
#define PAGE_WRITER_MAX_PER_RUN		128

/** Number of page queues. */
#define PAGE_QUEUE_COUNT		3

/** Maximum number of memory ranges. */
#define MEMORY_RANGE_MAX		32

/** Total usable page count. */
static page_num_t total_page_count = 0;

/** Allocated page queues. */
static page_queue_t page_queues[PAGE_QUEUE_COUNT];

/** Free page list. */
static page_freelist_t free_page_lists[PAGE_FREE_LIST_COUNT];
static MUTEX_DECLARE(free_page_lock, 0);

/** Physical memory ranges. */
static memory_range_t memory_ranges[MEMORY_RANGE_MAX];
static size_t memory_range_count = 0;

/** Free memory range descriptors for early page allocations. */
static boot_range_t boot_ranges[MEMORY_RANGE_MAX] __init_data;
static size_t boot_range_count __init_data = 0;

/** Whether the physical memory manager has been initialized. */
bool page_init_done = false;

/** Page writer thread.
 * @param arg1		Unused.
 * @param arg2		Unused. */
static void page_writer(void *arg1, void *arg2) {
	page_queue_t *queue = &page_queues[PAGE_STATE_MODIFIED];
	LIST_DECLARE(marker);
	size_t written;
	page_t *page;

	while(true) {
		/* TODO: When low on memory, should write pages more often. */
		delay(PAGE_WRITER_INTERVAL);

		/* Place the marker at the beginning of the queue to begin with. */
		written = 0;
		spinlock_lock(&queue->lock);
		list_prepend(&queue->pages, &marker);

		/* Write pages until we've reached the maximum number of pages
		 * per iteration, or until we reach the end of the queue. */
		while(written < PAGE_WRITER_MAX_PER_RUN && marker.next != &queue->pages) {
			/* Take the page and move the marker after it. */
			page = list_next(&marker, page_t, header);
			list_add_after(&page->header, &marker);
			spinlock_unlock(&queue->lock);

			/* Write out the page. */
			if(vm_cache_flush_page(page)) {
				dprintf("page: page writer wrote page 0x%" PRIxPHYS "\n", page->addr);
				written++;
			}

			spinlock_lock(&queue->lock);
		}

		/* Remove the marker and unlock. */
		list_remove(&marker);
		spinlock_unlock(&queue->lock);
	}
}

#if 0
/** Reclaim cached pages.
 * @param level		Resource level. */
static void vm_cache_reclaim(int level) {
	page_queue_t *queue = &page_queues[PAGE_QUEUE_CACHED];
	vm_page_t *page;
	size_t count = 0;

	spinlock_lock(&queue->lock);

	/* Work out how many pages to free. */
	switch(level) {
	case RESOURCE_LEVEL_ADVISORY:
		count = queue->count / 8;
		break;
	case RESOURCE_LEVEL_LOW:
		count = queue->count / 4;
		break;
	case RESOURCE_LEVEL_CRITICAL:
		count = queue->count;
		break;
	}

	/* Reclaim the pages. */
	while(count--) {
		page = list_first(&queue->pages, vm_page_t, header);
		spinlock_unlock(&queue->lock);
		vm_cache_evict_page(page);
		spinlock_lock(&queue->lock);
	}

	spinlock_unlock(&queue->lock);
}
#endif

/** Append page onto the end of a page queue.
 * @param index		Queue index to append to.
 * @param page		Page to push. */
static inline void page_queue_append(unsigned index, page_t *page) {
	assert(list_empty(&page->header));

	spinlock_lock(&page_queues[index].lock);
	list_append(&page_queues[index].pages, &page->header);
	page_queues[index].count++;
	spinlock_unlock(&page_queues[index].lock);
}

/** Remove a page from a page queue queue.
 * @param index		Queue index to remove from.
 * @param page		Page to remove. */
static inline void page_queue_remove(unsigned index, page_t *page) {
	spinlock_lock(&page_queues[index].lock);
	list_remove(&page->header);
	page_queues[index].count--;
	spinlock_unlock(&page_queues[index].lock);
}

/** Remove a page from the queue it currently belongs to.
 * @param page		Page to remove (should not be free). */
static void remove_page_from_current_queue(page_t *page) {
	/* Check that we have a valid current state. */
	if(unlikely(page->state >= PAGE_QUEUE_COUNT))
		fatal("Page 0x%" PRIxPHYS " has invalid state (%u)\n", page->addr, page->state);

	page_queue_remove(page->state, page);
}

/** Set the state of a page.
 * @param page		Page to set the state of. Must not currently be free.
 * @param state		New state for the page. Must not be PAGE_STATE_FREE,
 *			pages must be freed through page_free(). */
void page_set_state(page_t *page, unsigned state) {
	/* Remove from current queue. */
	remove_page_from_current_queue(page);

	if(unlikely(state >= PAGE_QUEUE_COUNT))
		fatal("Setting invalid state on 0x%" PRIxPHYS " (%u)\n", page->addr, state);

	/* Set new state and push on the new queue. */
	page->state = state;
	page_queue_append(state, page);
}

/** Look up the page structure for a physical address.
 * @param addr		Address to look up.
 * @return		Pointer to page structure if found, null if not. */
page_t *page_lookup(phys_ptr_t addr) {
	size_t i;

	assert(!(addr % PAGE_SIZE));

	for(i = 0; i < memory_range_count; i++) {
		if(addr >= memory_ranges[i].start && addr < memory_ranges[i].end)
			return &memory_ranges[i].pages[(addr - memory_ranges[i].start)
				>> PAGE_WIDTH];
	}

	return NULL;
}

/** Allocate a page.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to structure for allocated page. */
page_t *page_alloc(unsigned mmflag) {
	void *mapping;
	page_t *page;
	unsigned i;

	assert((mmflag & (MM_WAIT | MM_ATOMIC)) != (MM_WAIT | MM_ATOMIC));

	/* Acquire the lock and wire the current thread to this CPU so that the
	 * entire operation is performed on this CPU. */
	thread_wire(curr_thread);
	mutex_lock(&free_page_lock);

	/* Attempt to allocate from each of the lists. */
	for(i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
		if(list_empty(&free_page_lists[i].pages))
			continue;

		/* Get the page and mark it as allocated. */
		page = list_first(&free_page_lists[i].pages, page_t, header);
		list_remove(&page->header);
		page->state = PAGE_STATE_ALLOCATED;

		/* No longer require the lock. Must be released before
		 * attempting to zero the page as that might require another
		 * allocation, which would lead to a nested locking error. */
		mutex_unlock(&free_page_lock);

		/* Put the page onto the allocated queue. */
		page_queue_append(PAGE_STATE_ALLOCATED, page);

		/* If we require a zero page, clear it now. */
		if(mmflag & MM_ZERO) {
			mapping = phys_map(page->addr, PAGE_SIZE, mmflag & MM_FLAG_MASK);
			if(unlikely(!mapping)) {
				page_free(page);
				thread_unwire(curr_thread);
				return NULL;
			}

			memset(mapping, 0, PAGE_SIZE);
			phys_unmap(mapping, PAGE_SIZE, false);
		}

		thread_unwire(curr_thread);

		dprintf("page: allocated page 0x%" PRIxPHYS " (list: %u)\n", page->addr, i);
		return page;
	}

	// TODO: Reclaim/wait for memory.
	if(mmflag & MM_BOOT) {
		fatal("Unable to satisfy boot page allocation");
	} else if(mmflag & MM_WAIT) {
		/* TODO: Try harder. */
		fatal("TODO: Reclaim/wait for memory");
	}

	mutex_unlock(&free_page_lock);
	thread_unwire(curr_thread);
	return NULL;
}

/** Internal page freeing code.
 * @param page		Page to free. */
static void page_free_internal(page_t *page) {
	assert(!refcount_get(&page->count));

	/* Reset the page structure to a clear state. */
	page->state = PAGE_STATE_FREE;
	page->modified = false;
	page->cache = NULL;

	/* Push it onto the appropriate list. */
	list_prepend(&free_page_lists[memory_ranges[page->range].freelist].pages, &page->header);
}

/** Free a page.
 * @param page		Page to free. */
void page_free(page_t *page) {
	if(unlikely(page->state == PAGE_STATE_FREE))
		fatal("Attempting to free already free page 0x%" PRIxPHYS, page->addr);

	/* Remove from current queue. */
	remove_page_from_current_queue(page);

	mutex_lock(&free_page_lock);
	page_free_internal(page);
	mutex_unlock(&free_page_lock);

	dprintf("page: freed page 0x%" PRIxPHYS " (list: %u)\n", page->addr,
		memory_ranges[page->range].freelist);
}

/** Create a copy of a page.
 * @param page		Page to copy.
 * @param mmflag	Allocation flags.
 * @return		Pointer to new page structure on success, NULL on
 *			failure. */
page_t *page_copy(page_t *page, unsigned mmflag) {
	page_t *dest;

	assert(page);

	dest = page_alloc(mmflag);
	if(unlikely(!dest)) {
		return NULL;
	} else if(unlikely(!phys_copy(dest->addr, page->addr, mmflag))) {
		page_free(dest);
		return NULL;
	}

	return dest;
}

/** Fast path for phys_alloc() (1 page, only minimum/maximum address).
 * @param minaddr	Minimum address of the range.
 * @param maxaddr	Maximum address of the range.
 * @return		Pointer to first page in range if found, null if not. */
static page_t *phys_alloc_fastpath(phys_ptr_t minaddr, phys_ptr_t maxaddr) {
	page_freelist_t *list;
	phys_ptr_t base, end;
	page_t *page;
	unsigned i;

	/* Maximum of 2 possible partial fits. */
	unsigned partial_fits[2];
	unsigned partial_fit_count = 0;

	/* On the first pass through, we try to allocate from all free lists
	 * that are guaranteed to fit these constraints. */
	for(i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
		list = &free_page_lists[i];
		if(!list->minaddr && !list->maxaddr)
			continue;

		base = MAX(minaddr, list->minaddr);
		end = MIN(maxaddr - 1, list->maxaddr - 1);

		if(base == list->minaddr && end == (list->maxaddr - 1)) {
			/* Exact fit. */
			dprintf("page: free list %u can satisfy [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
				i, minaddr, maxaddr);
			if(list_empty(&list->pages))
				continue;

			return list_first(&list->pages, page_t, header);
		} else if(end > base) {
			/* Partial fit, record to check in the second pass. */
			partial_fits[partial_fit_count++] = i;
		}
	}

	/* Check any lists that were determined to be a partial fit. */
	for(i = 0; i < partial_fit_count; i++) {
		dprintf("page: free list %u can partially satisfy [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
			partial_fits[i], minaddr, maxaddr);

		/* Check if there are any pages that can fit. */
		list = &free_page_lists[partial_fits[i]];
		LIST_FOREACH(&list->pages, iter) {
			page = list_entry(iter, page_t, header);

			if(page->addr >= minaddr && (!maxaddr || page->addr < maxaddr))
				return page;
		}
	}

	return NULL;
}

/** Slow path for phys_alloc().
 * @param count		Number of pages to allocate.
 * @param align		Required alignment of the range.
 * @param boundary	Boundary that the range cannot cross.
 * @param minaddr	Minimum start address of the range.
 * @param maxaddr	Maximum end address of the range.
 * @return		Pointer to first page in range if found, null if not. */
static page_t *phys_alloc_slowpath(page_num_t count, phys_ptr_t align,
	phys_ptr_t boundary, phys_ptr_t minaddr, phys_ptr_t maxaddr)
{
	phys_ptr_t match_start, match_end, start, end;
	page_num_t index, total, j;
	memory_range_t *range;
	size_t i;

	if(boundary != 0)
		fatal("TODO: Implement boundary constraint");

	if(!align)
		align = PAGE_SIZE;

	/* Lovely and slow! Scan each page in each physical range to try to
	 * find a set of pages that satisfy the allocation. Because we hold
	 * the free pages lock, it is guaranteed that no pages will enter or
	 * leave the free state (they can still move between other states)
	 * while we are working. */
	for(i = 0; i < memory_range_count; i++) {
		range = &memory_ranges[i];
		total = 0;

		/* Check if this range contains pages in the requested range. */
		match_start = MAX(minaddr, range->start);
		match_end = MIN(maxaddr - 1, range->end - 1);
		if(match_end <= match_start)
			continue;

		/* Scan pages in the range. */
		start = (match_start - range->start) / PAGE_SIZE;
		end = ((match_end - range->start) + 1) / PAGE_SIZE;
		for(j = start; j < end; j++) {
			if(!total) {
				/* Check if this is a suitable starting page. */
				if(range->pages[j].addr & (align - 1))
					continue;

				index = j;
			}

			/* Check if the page is free. */
			if(range->pages[j].state != PAGE_STATE_FREE) {
				total = 0;
				continue;
			}

			if(++total == count)
				return &range->pages[index];
		}
	}

	return NULL;
}

/**
 * Allocate a range of contiguous physical memory.
 *
 * Allocates a range of contiguous physical memory, with constraints on the
 * location of the allocation. All arguments must be a multiple of the system
 * page size, and any constraints which are not required should be specified
 * as 0. It is intended that this function is not used regularly, for example
 * within driver initialization routines, as it is not optimised for fast
 * allocations. It is, however, optimised for single-page allocations with only
 * certain platform-specific minimum/maximum address constraints, for example
 * below 16MB or below 4GB on the PC platform.
 *
 * @param size		Size of the range to allocate.
 * @param align		Required alignment of the range (power of 2).
 * @param boundary	Boundary that the range cannot cross (power of 2).
 * @param minaddr	Minimum start address of the range.
 * @param maxaddr	Maximum end address of the range.
 * @param mmflag	Allocation behaviour flags.
 * @param basep		Where to store address of the allocation on success.
 *
 * @return		Status code describing the result of the operation.
 */
status_t phys_alloc(phys_size_t size, phys_ptr_t align, phys_ptr_t boundary,
	phys_ptr_t minaddr, phys_ptr_t maxaddr, unsigned mmflag,
	phys_ptr_t *basep)
{
	page_num_t count, i;
	page_t *pages;
	void *mapping;

	assert(size);
	assert(!(size % PAGE_SIZE));
	assert(!(align % PAGE_SIZE));
	assert(!align || IS_POW2(align));
	assert(!(boundary % PAGE_SIZE));
	assert(!boundary || IS_POW2(boundary));
	assert(!(minaddr % PAGE_SIZE));
	assert(!(maxaddr % PAGE_SIZE));
	assert(!(minaddr && maxaddr) || maxaddr > minaddr);
	assert((mmflag & (MM_WAIT | MM_ATOMIC)) != (MM_WAIT | MM_ATOMIC));

	/* Work out how many pages we need to allocate. */
	count = size / PAGE_SIZE;

	/* Acquire the lock and wire the current thread to this CPU so that the
	 * entire operation is performed on this CPU. */
	thread_wire(curr_thread);
	mutex_lock(&free_page_lock);

	/* Single-page allocations with no constraints or only minaddr/maxaddr
	 * constraints can be performed quickly. */
	pages = (count == 1 && !align && !boundary)
		? phys_alloc_fastpath(minaddr, maxaddr)
		: phys_alloc_slowpath(count, align, boundary, minaddr, maxaddr);
	if(unlikely(!pages)) {
		if(mmflag & MM_BOOT) {
			fatal("Unable to satisfy boot allocation of %zu page(s)", count);
		} else if(mmflag & MM_WAIT) {
			fatal("TODO: Reclaim/wait for memory");
		}

		mutex_unlock(&free_page_lock);
		thread_unwire(curr_thread);
		return STATUS_NO_MEMORY;
	}

	/* Remove the pages from the free list and mark them as allocated. */
	for(i = 0; i < count; i++) {
		list_remove(&pages[i].header);
		pages[i].state = PAGE_STATE_ALLOCATED;
	}

	/* Release the lock (see locking rules). */
	mutex_unlock(&free_page_lock);

	/* Put the pages onto the allocated queue. Pages will have already been
	 * marked as allocated. */
	for(i = 0; i < count; i++)
		page_queue_append(PAGE_STATE_ALLOCATED, &pages[i]);

	/* If we require the range to be zero, clear it now. */
	if(mmflag & MM_ZERO) {
		mapping = phys_map(pages->addr, size, mmflag & MM_FLAG_MASK);
		if(unlikely(!mapping)) {
			phys_free(pages->addr, size);
			thread_unwire(curr_thread);
			return STATUS_NO_MEMORY;
		}

		memset(mapping, 0, size);
		phys_unmap(mapping, size, false);
	}

	thread_unwire(curr_thread);

	dprintf("page: allocated page range [0x%" PRIxPHYS ",0x%" PRIxPHYS ")\n",
		pages->addr, pages->addr + size);
	*basep = pages->addr;
	return STATUS_SUCCESS;
}

/** Free a range of physical memory.
 * @param base		Base address of range.
 * @param size		Size of range. */
void phys_free(phys_ptr_t base, phys_size_t size) {
	page_t *pages;
	page_num_t i;

	assert(!(base % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));
	assert(size);
	assert((base + size) > base);

	pages = page_lookup(base);
	if(unlikely(!pages))
		fatal("Invalid base address 0x%" PRIxPHYS, base);

	/* Ranges allocated by phys_alloc() will not span across a physical
	 * range boundary. Check that the caller is not trying to free across
	 * one. */
	if(unlikely((base + size) > memory_ranges[pages->range].end))
		fatal("Invalid free across range boundary");

	/* Remove each page in the range from its current queue. */
	for(i = 0; i < (size / PAGE_SIZE); i++) {
		if(unlikely(pages[i].state == PAGE_STATE_FREE)) {
			fatal("Page 0x%" PRIxPHYS " in range [0x%" PRIxPHYS
				",0x%" PRIxPHYS ") already free", pages[i].addr,
				base, base + size);
		}

		remove_page_from_current_queue(&pages[i]);
	}

	mutex_lock(&free_page_lock);

	/* Free each page. */
	for(i = 0; i < size / PAGE_SIZE; i++)
		page_free_internal(&pages[i]);

	mutex_unlock(&free_page_lock);

	dprintf("page: freed page range [0x%" PRIxPHYS ",0x%" PRIxPHYS ") (list: %u)\n",
		base, base + size, memory_ranges[pages->range].freelist);
}

/** Get physical memory usage statistics.
 * @param stats		Structure to fill in. */
void page_stats_get(page_stats_t *stats) {
	stats->total = total_page_count * PAGE_SIZE;
	stats->allocated = page_queues[PAGE_STATE_ALLOCATED].count * PAGE_SIZE;
	stats->modified = page_queues[PAGE_STATE_MODIFIED].count * PAGE_SIZE;
	stats->cached = page_queues[PAGE_STATE_CACHED].count * PAGE_SIZE;
	stats->free = stats->total - stats->allocated - stats->modified - stats->cached;
}

/** Print details about physical memory usage.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDB status code. */
static kdb_status_t kdb_cmd_page(int argc, char **argv, kdb_filter_t *filter) {
	page_stats_t stats;
	uint64_t addr;
	page_t *page;
	size_t i;

	if(kdb_help(argc, argv)) {
		kdb_printf("Usage: %s [<addr>]\n\n", argv[0]);

		kdb_printf("Prints out a list of all usable page ranges and information about physical\n");
		kdb_printf("memory usage, or details of a single page.\n");
		return KDB_SUCCESS;
	} else if(argc != 1 && argc != 2) {
		kdb_printf("Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDB_FAILURE;
	}

	if(argc == 2) {
		if(kdb_parse_expression(argv[1], &addr, NULL) != KDB_SUCCESS) {
			return KDB_FAILURE;
		} else if(addr % PAGE_SIZE) {
			kdb_printf("Address must be page aligned.\n");
			return KDB_FAILURE;
		} else if(!(page = page_lookup((phys_ptr_t)addr))) {
			kdb_printf("404 Page Not Found\n");
			return KDB_FAILURE;
		}

		kdb_printf("Page 0x%" PRIxPHYS " (%p) (Range: %u)\n", page->addr, page, page->range);
		kdb_printf("=================================================\n");
		kdb_printf("State:    %d\n", page->state);
		kdb_printf("Modified: %d\n", page->modified);
		kdb_printf("Count:    %d\n", page->count);
		kdb_printf("Cache:    %p\n", page->cache);
		kdb_printf("Offset:   %" PRIu64 "\n", page->offset);
	} else {
		kdb_printf("Start              End                Freelist Pages\n");
		kdb_printf("=====              ===                ======== =====\n");

		for(i = 0; i < memory_range_count; i++) {
			kdb_printf("0x%-16" PRIxPHYS " 0x%-16" PRIxPHYS " %-8u %p\n",
				memory_ranges[i].start, memory_ranges[i].end,
				memory_ranges[i].freelist, memory_ranges[i].pages);
		}

		page_stats_get(&stats);
		kdb_printf("\nUsage statistics\n");
		kdb_printf("================\n");
		kdb_printf("Total:     %" PRIu64 " KiB\n", stats.total / 1024);
		kdb_printf("Allocated: %" PRIu64 " KiB\n", stats.allocated / 1024);
		kdb_printf("Modified:  %" PRIu64 " KiB\n", stats.modified / 1024);
		kdb_printf("Cached:    %" PRIu64 " KiB\n", stats.cached / 1024);
		kdb_printf("Free:      %" PRIu64 " KiB\n", stats.free / 1024);
	}

	return KDB_SUCCESS;
}

/** Add a new range of physical memory.
 * @note		Ranges must be added in lowest to highest order!
 * @param start		Start of range.
 * @param end		End of range.
 * @param freelist	Free page list index for pages in the range. */
__init_text void page_add_memory_range(phys_ptr_t start, phys_ptr_t end, unsigned freelist) {
	page_freelist_t *list = &free_page_lists[freelist];
	memory_range_t *range, *prev;

	/* Increase the total page count. */
	total_page_count += (end - start) / PAGE_SIZE;

	/* Update the freelist to include this range. */
	if(!list->minaddr && !list->maxaddr) {
		list->minaddr = start;
		list->maxaddr = end;
	} else {
		if(start < list->minaddr)
			list->minaddr = start;
		if(end > list->maxaddr)
			list->maxaddr = end;
	}

	/* If we're contiguous with the previously recorded range (if any) and
	 * have the same free list index, just append to it, else add a new
	 * range. */
	if(memory_range_count) {
		prev = &memory_ranges[memory_range_count - 1];
		if(start == prev->end && freelist == prev->freelist) {
			prev->end = end;
			return;
		}
	}

	if(memory_range_count >= MEMORY_RANGE_MAX)
		fatal("Too many physical memory ranges");

	range = &memory_ranges[memory_range_count++];
	range->start = start;
	range->end = end;
	range->freelist = freelist;
}

/** Perform an early page allocation.
 * @return		Physical address of page allocated. */
phys_ptr_t page_early_alloc(void) {
	phys_ptr_t ret;
	size_t i;

	/* Search for a range with free pages. */
	for(i = 0; i < boot_range_count; i++) {
		if(boot_ranges[i].allocated < boot_ranges[i].size) {
			ret = boot_ranges[i].start + boot_ranges[i].allocated;
			boot_ranges[i].allocated += PAGE_SIZE;
			dprintf("page: allocated early page 0x%" PRIxPHYS "\n", ret);
			return ret;
		}
	}

	fatal("Exhausted available memory during boot");
}

/** Sort comparison function the boot range array. */
static __init_text int boot_range_compare(const void *a, const void *b) {
	const boot_range_t *first = (const boot_range_t *)a;
	const boot_range_t *second = (const boot_range_t *)b;

	if(first->freelist == second->freelist) {
		return (first->start < second->start) ? -1 : (first->start > second->start);
	} else {
		return (first->freelist < second->freelist) ? -1 : 1;
	}
}

/** Perform early physical memory manager initialization. */
__init_text void page_early_init(void) {
	page_freelist_t *list;
	unsigned i;

	/* Initialize page queues and freelists. */
	for(i = 0; i < PAGE_QUEUE_COUNT; i++) {
		list_init(&page_queues[i].pages);
		page_queues[i].count = 0;
		spinlock_init(&page_queues[i].lock, "page_queue_lock");
	}
	for(i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
		list_init(&free_page_lists[i].pages);
		free_page_lists[i].minaddr = 0;
		free_page_lists[i].maxaddr = 0;
	}

	/* First step is to call into platform-specific code to parse the
	 * memory map provided by the loader and separate it further as
	 * required (i.e. into different free lists). */
	platform_page_init();

	/* And here we have the fun of early memory management bringup. We want
	 * to map pages during our initialization, but to do that the MMU code
	 * needs to allocate pages. Therefore, we have an early page allocation
	 * system for use during initialization that just grabs the first free
	 * page it can find from the memory ranges given by the boot loader.
	 * The real initialization code will then mark all pages allocated
	 * through that function as allocated. */
	KBOOT_ITERATE(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
		if(boot_range_count == MEMORY_RANGE_MAX)
			fatal("Memory map contains too many ranges");

		boot_ranges[boot_range_count].start = range->start;
		boot_ranges[boot_range_count].size = range->size;
		boot_ranges[boot_range_count].allocated
			= (range->type == KBOOT_MEMORY_FREE) ? 0 : range->size;

		/* Match up the range against a freelist. Not entirely correct
		 * for ranges that straddle across multiples lists, so we just
		 * always select the lowest priority (highest index) list. */
		for(i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
			list = &free_page_lists[i];
			if(!list->minaddr && !list->maxaddr)
				continue;

			if(range->start <= (list->maxaddr - 1)
				&& list->minaddr <= (range->start + range->size - 1))
			{
				boot_ranges[boot_range_count].freelist = i;
			}
		}

		boot_range_count++;
	}

	/* Sort the array of boot ranges by lowest to highest freelist index
	 * so that early allocations honor free list priorities. */
	qsort(boot_ranges, boot_range_count, sizeof(boot_range_t), boot_range_compare);
}

/** Initialize the physical memory manager. */
__init_text void page_init(void) {
	size_t pages_size, i, j, size;
	ptr_t addr;
	page_num_t count;
	page_t *page;
	unsigned index;

	kprintf(LOG_NOTICE, "page: usable physical memory ranges:\n");
	for(i = 0; i < memory_range_count; i++) {
		kprintf(LOG_NOTICE, " 0x%016" PRIxPHYS " - 0x%016" PRIxPHYS " (%u)\n",
			memory_ranges[i].start, memory_ranges[i].end,
			memory_ranges[i].freelist);
	}
	kprintf(LOG_NOTICE, "page: free list coverage:\n");
	for(i = 0; i < PAGE_FREE_LIST_COUNT; i++) {
		kprintf(LOG_NOTICE, " %u: 0x%016" PRIxPHYS " - 0x%016" PRIxPHYS "\n",
			i, free_page_lists[i].minaddr, free_page_lists[i].maxaddr);
	}

	/* Determine how much space we need for the page database. */
	pages_size = ROUND_UP(sizeof(page_t) * total_page_count, PAGE_SIZE);
	kprintf(LOG_NOTICE, "page: have %zu pages, using %" PRIuPHYS "KiB for page database\n",
		total_page_count, pages_size / 1024);
	if(pages_size > KERNEL_PDB_SIZE)
		fatal("Available RAM exceeds maximum page database size");

	mmu_context_lock(&kernel_mmu_context);

	/* Allocate and map the database. */
	for(i = 0; i < pages_size; i += PAGE_SIZE) {
		mmu_context_map(&kernel_mmu_context, KERNEL_PDB_BASE + i,
			page_early_alloc(), MMU_MAP_WRITE, MM_BOOT);
	}

	mmu_context_unlock(&kernel_mmu_context);

	addr = KERNEL_PDB_BASE;

	/* Now for each memory range we have, create page structures. */
	for(i = 0; i < memory_range_count; i++) {
		count = (memory_ranges[i].end - memory_ranges[i].start) / PAGE_SIZE;
		size = sizeof(page_t) * count;
		memory_ranges[i].pages = (page_t *)addr;
		addr += size;

		/* Initialize each of the pages. */
		memset(memory_ranges[i].pages, 0, size);
		for(j = 0; j < count; j++) {
			page = &memory_ranges[i].pages[j];
			list_init(&page->header);
			page->addr = memory_ranges[i].start + ((phys_ptr_t)j * PAGE_SIZE);
			page->range = i;
		}
	}

	/* Finally, set the state of each page based on the boot allocation
	 * information. */
	for(i = 0; i < boot_range_count; i++) {
		for(j = 0; j < boot_ranges[i].size; j += PAGE_SIZE) {
			page = page_lookup(boot_ranges[i].start + j);
			assert(page);

			if(j >= boot_ranges[i].allocated) {
				page->state = PAGE_STATE_FREE;
				index = memory_ranges[page->range].freelist;
				list_append(&free_page_lists[index].pages, &page->header);
			} else {
				page->state = PAGE_STATE_ALLOCATED;
				page_queue_append(PAGE_STATE_ALLOCATED, page);
			}
		}
	}

	kdb_register_command("page", "Display physical memory usage information.",
		kdb_cmd_page);

	page_init_done = true;
}

/** Initialize the page daemons. */
__init_text void page_daemon_init(void) {
	status_t ret;

	ret = thread_create("page_writer", NULL, 0, page_writer, NULL, NULL, NULL);
	if(ret != STATUS_SUCCESS)
		fatal("Could not start page writer (%d)", ret);
}

/** Reclaim memory no longer in use after kernel initialization. */
__init_text void page_late_init(void) {
	phys_ptr_t addr, init_start, init_end;
	kboot_tag_core_t *core;
	size_t reclaimed = 0;

	/* Calculate the location and size of the initialization section. */
	core = kboot_tag_iterate(KBOOT_TAG_CORE, NULL);
	init_start = ((ptr_t)__init_seg_start - KERNEL_VIRT_BASE) + core->kernel_phys;
	init_end = ((ptr_t)__init_seg_end - KERNEL_VIRT_BASE) + core->kernel_phys;

	/* It's OK for us to reclaim despite the fact that the KBoot data is
	 * contained in memory that will be reclaimed, as nothing should make
	 * any allocations or write to reclaimed memory while this is
	 * happening. */
	KBOOT_ITERATE(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
		if(range->type != KBOOT_MEMORY_FREE && range->type != KBOOT_MEMORY_ALLOCATED) {
			/* Must individually free each page in the range, as
			 * the KBoot range could be split across more than one
			 * of our internal ranges, and frees across range
			 * boundaries are not allowed. */
			for(addr = range->start; addr < range->start + range->size; addr += PAGE_SIZE)
				phys_free(addr, PAGE_SIZE);

			reclaimed += range->size;
		}
	}

	/* Free the initialization data. Same as above applies. */
	for(addr = init_start; addr < init_end; addr += PAGE_SIZE)
		phys_free(addr, PAGE_SIZE);

	reclaimed += (init_end - init_start);

	kprintf(LOG_NOTICE, "page: reclaimed %zu KiB of unneeded memory\n", reclaimed / 1024);
}
