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
 *
 * The functions in this file manage all usable physical memory in the system.
 * There are several parts to the physical memory manager, which will be
 * described below.
 *
 * Firstly, a vmem arena is used to handle the allocation of ranges of physical
 * memory. This allows fast allocations of groups of pages, and also allows
 * constrained allocations.
 *
 * Secondly, each usable page in the system has a vm_page structure associated
 * with it, which tracks how pages are being used. Each usable memory range in
 * the range list from the bootloader is stored in a global array and has an
 * array of vm_page structures allocated for them. This global array can then
 * be used to quickly look up the structure associated with a physical address.
 *
 * Thirdly, there are a number of queues that a page can be placed in depending
 * on its state, and these queues are used for various purposes. Below is a
 * description of each queue:
 *  - Modified: A queue of pages which have been modified and need to be
 *    written to their source. There is a special thread (the page writer) that
 *    periodically takes pages off this queue and writes them. This
 *    functionality is used by the file cache to ensure that modifications to
 *    files get written to the filesystem soon, rather than staying in memory
 *    for a long time without being written.
 *  - Cached: A queue of pages that are not currently mapped, but are holding
 *    cached data. Pages are taken from this queue and freed up when the number
 *    of free pages gets low.
 *  - Pageable: A queue of pages that are currently mapped into an address
 *    space, but can be paged out if necessary. When the system is low on
 *    free pages, and the cached/modified queues are empty, pages will start
 *    being taken from this queue and paged out.
 * When a page is free, or allocated but not pageable it will not be in any
 * queues.  The movement of pages between queues is mostly left up to the users
 * of the pages.
 *
 * There are two sets of functions for allocating/freeing pages. Firstly, there
 * are the functions defined in mm/page.h, which return physical addresses,
 * and are not concerned with vm_page structures. These are useful where access
 * to the vm_page structure for a page is not necessary, for example in device
 * drivers or in the kernel heap manager. Secondly, the functions defined in
 * mm/vm.h deal with vm_page structures. The allocation/free functions both
 * perform exactly the same, the only difference being the type of the return
 * value.
 *
 * @note		Although you're supposed to use matching vmem functions
 *			this code doesn't, and just provides page_free() that
 *			can be used with constrained allocations, as there is
 *			only a problem with using non-matching calls if the
 *			arena uses quantum caching.
 */

#include <arch/memmap.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kargs.h>
#include <kdbg.h>
#include <vmem.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Structure containing information about a page range. */
typedef struct page_range {
	phys_ptr_t start;		/**< Start of range. */
	phys_ptr_t end;			/**< End of range. */
	vm_page_t *pages;		/**< Array of page structures. */
	bool reclaim;			/**< Whether the range needs to be reclaimed after boot. */
} page_range_t;

/** Structure containing a page queue. */
typedef struct page_queue {
	list_t pages;			/**< List of pages. */
	size_t count;			/**< Number of pages in the queue. */
	spinlock_t lock;		/**< Lock to protect the queue. */
} page_queue_t;

extern char __init_start[], __init_end[];

/** Arena used for page allocations. */
static vmem_t page_arena;

/** Array of all page ranges. */
static page_range_t page_ranges[KERNEL_ARGS_RANGES_MAX];
static size_t page_range_count = 0;

/** Array of page queues. */
static page_queue_t page_queues[PAGE_QUEUE_COUNT];

#if CONFIG_DEBUG
/** Ensure that a free page is in the correct state.
 * @param page		Page to check. */
static inline void check_free_page(vm_page_t *page) {
	assert(!(page->addr % PAGE_SIZE));
	assert(!refcount_get(&page->count));
	assert(!page->modified);
	assert(!page->object);
	assert(!page->amap);
	assert(!page->queue);
	assert(list_empty(&page->header));
}

/** Ensure that a range of free pages are in the correct state.
 * @param base		Base of page range.
 * @param count		Number of pages in range. */
static inline void check_free_pages(phys_ptr_t base, size_t count) {
	vm_page_t *pages;
	size_t i;

	/* Check that all pages in the range have the correct state. */
	pages = vm_page_lookup(base);
	if(likely(pages)) {
		for(i = 0; i < count; i++) {
			check_free_page(&pages[i]);
		}
	}
}
#endif

/** Allocate a range of pages.
 * @param count		Number of pages to allocate.
 * @param pmflag	Flags to control allocation behaviour.
 * @return		Pointer to page structure for the base of the range
 *			allocated, or NULL on failure. */
vm_page_t *vm_page_alloc(size_t count, int pmflag) {
	vm_page_t *pages;
	phys_ptr_t base;

	if(!(base = page_xalloc(count, 0, 0, 0, pmflag))) {
		return NULL;
	} else if(unlikely(!(pages = vm_page_lookup(base)))) {
		fatal("Could not look up pages for [%" PRIpp ", %" PRIpp ")",
		      base, base + (count * PAGE_SIZE));
	}

	return pages;
}

/** Free a range of pages.
 *
 * Frees a range of pages. Parameters passed to this function must exactly
 * match those of the original allocation, i.e. you cannot allocate a range
 * of 6 pages then try to only free 4 of them.
 *
 * @param pages		Pointer to structure for base of range.
 * @param count		Number of pages to free.
 */
void vm_page_free(vm_page_t *pages, size_t count) {
	size_t i;

	/* Clear any flags that shouldn't be set and perform checks. */
	for(i = 0; i < count; i++) {
		pages[i].modified = false;
#if CONFIG_DEBUG
		check_free_page(&pages[i]);
#endif
	}

	vmem_free(&page_arena, pages[0].addr, count * PAGE_SIZE);
}

/** Create a copy of a page.
 * @param page		Page to copy.
 * @param mmflag	Allocation flags.
 * @return		Pointer to new page structure on success, NULL on
 *			failure. */
vm_page_t *vm_page_copy(vm_page_t *page, int mmflag) {
	vm_page_t *dest;

	assert(page);

	if(!(dest = vm_page_alloc(1, mmflag))) {
		return NULL;
	} else if(page_copy(dest->addr, page->addr, mmflag) != 0) {
		vm_page_free(dest, 1);
		return NULL;
	}

	return dest;
}

/** Insert a page into a page queue.
 * @param page		Page to queue. Will be detached from current queue.
 * @param queue		ID of queue to insert into. */
void vm_page_queue(vm_page_t *page, size_t queue) {
	assert(queue < PAGE_QUEUE_COUNT);

	vm_page_dequeue(page);
	page->queue = &page_queues[queue];

	spinlock_lock(&page_queues[queue].lock);
	list_append(&page_queues[queue].pages, &page->header);
	page_queues[queue].count++;
	spinlock_unlock(&page_queues[queue].lock);
}

/** Remove a page from any queue it is in.
 * @param page		Page to dequeue. */
void vm_page_dequeue(vm_page_t *page) {
	if(page->queue) {
		spinlock_lock(&page->queue->lock);
		list_remove(&page->header);
		page->queue->count--;
		spinlock_unlock(&page->queue->lock);

		page->queue = NULL;
	}
}

/** Look up the page structure for a physical address.
 * @param addr		Address to look up.
 * @return		Pointer to page structure if found, NULL if not. */
vm_page_t *vm_page_lookup(phys_ptr_t addr) {
	size_t i;

	assert(!(addr % PAGE_SIZE));

	for(i = 0; i < page_range_count; i++) {
		if(addr >= page_ranges[i].start && addr < page_ranges[i].end) {
			if(!page_ranges[i].pages) {
				return NULL;
			}
			return &page_ranges[i].pages[(addr - page_ranges[i].start) / PAGE_SIZE];
		}
	}

	return NULL;
}

/** Allocate a range of pages with constraints.
 *
 * Allocates a range of pages. Flags can be specified to modify the allocation
 * behaviour, and constraints can be specified to control where the allocation
 * is made.
 *
 * @param count		Number of pages to allocate.
 * @param align		Alignment of allocation.
 * @param minaddr	Minimum start address of the allocation.
 * @param maxaddr	Highest end address of the allocation.
 * @param pmflag	Flags to control allocation behaviour.
 *
 * @return		Base address of range allocated or 0 if unable to
 *			allocate.
 */
phys_ptr_t page_xalloc(size_t count, phys_ptr_t align, phys_ptr_t minaddr,
                       phys_ptr_t maxaddr, int pmflag) {
	phys_ptr_t base;
	void *mapping;

	if(!(base = vmem_xalloc(&page_arena, count * PAGE_SIZE, align, 0, 0, minaddr,
	                        maxaddr, (pmflag & MM_FLAG_MASK) & ~MM_FATAL))) {
		if(pmflag & MM_FATAL) {
			fatal("Could not perform mandatory allocation of %zu pages (1)", count);
		}
		return 0;
	}
#if CONFIG_DEBUG
	/* Perform checks on the page range to ensure that the pages are in
	 * the correct state. */
	check_free_pages(base, count);
#endif
	/* Map all of the pages into memory and zero them if needed. */
	if(pmflag & PM_ZERO) {
		thread_wire(curr_thread);

		mapping = page_phys_map(base, count * PAGE_SIZE, (pmflag & MM_FLAG_MASK) & ~MM_FATAL);
		if(!mapping) {
			if(pmflag & MM_FATAL) {
				fatal("Could not perform mandatory allocation of %zu pages (2)", count);
			}

			thread_unwire(curr_thread);
			vmem_free(&page_arena, base, count * PAGE_SIZE);
			return 0;
		}

		memset(mapping, 0, count * PAGE_SIZE);
		page_phys_unmap(mapping, count * PAGE_SIZE, false);
		thread_unwire(curr_thread);
	}

	dprintf("page: allocated page range [0x%" PRIpp ",0x%" PRIpp ")\n",
		base, base + (count * PAGE_SIZE));
	return base;
}

/** Allocate a range of pages.
 * @param count		Number of pages to allocate.
 * @param pmflag	Flags to control allocation behaviour.
 * @return		Base address of range allocated or 0 if unable to
 *			allocate. */
phys_ptr_t page_alloc(size_t count, int pmflag) {
	return page_xalloc(count, 0, 0, 0, pmflag);
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
	vm_page_t *page;

	assert(!(base % PAGE_SIZE));

	/* Go through vm_page_free() here as that performs various checks and
	 * ensures that all free pages are in exactly the same state. */
	if(!(page = vm_page_lookup(base))) {
		/* Assume if the page is not found then vm_page_init() hasn't
		 * been called yet. If the caller is passing in crap,
		 * vmem_free() will pick it up. */
		vmem_free(&page_arena, (vmem_resource_t)base, count * PAGE_SIZE);
		dprintf("page: freed page range [0x%" PRIpp ",0x%" PRIpp ")\n",
			base, base + (count * PAGE_SIZE));
	} else {
		vm_page_free(page, count);
	}
}

/** Copy the contents of a page.
 * @param dest		Destination page.
 * @param source	Source page.
 * @param mmflag	Allocation flags for mapping page in memory.
 * @return		0 on success, negative error code on failure. */
int page_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag) {
	void *mdest, *msrc;

	assert(!(dest % PAGE_SIZE));
	assert(!(source % PAGE_SIZE));

	thread_wire(curr_thread);

	if(!(mdest = page_phys_map(dest, PAGE_SIZE, mmflag))) {
		thread_unwire(curr_thread);
		return -ERR_NO_MEMORY;
	} else if(!(msrc = page_phys_map(source, PAGE_SIZE, mmflag))) {
		page_phys_unmap(mdest, PAGE_SIZE, false);
		thread_unwire(curr_thread);
		return -ERR_NO_MEMORY;
	}

	memcpy(mdest, msrc, PAGE_SIZE);
	page_phys_unmap(msrc, PAGE_SIZE, false);
	page_phys_unmap(mdest, PAGE_SIZE, false);
	thread_unwire(curr_thread);
	return 0;
}

/** Get physical memory usage statistics.
 * @param stats		Structure to fill in. */
void page_stats_get(page_stats_t *stats) {
	stats->total = page_arena.total_size;
	stats->modified = page_queues[PAGE_QUEUE_MODIFIED].count * PAGE_SIZE;
	stats->cached = page_queues[PAGE_QUEUE_CACHED].count * PAGE_SIZE;
	stats->free = page_arena.total_size - page_arena.used_size;
	stats->allocated = page_arena.used_size - stats->modified - stats->cached;
}

/** Print details about physical memory usage.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success, KDBG_FAIL on failure. */
int kdbg_cmd_page(int argc, char **argv) {
	page_stats_t stats;
	vm_page_t *page;
	unative_t addr;
	int queue = -1;
	size_t i;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s [<addr>]\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints out a list of all usable page ranges and information about physical\n");
		kprintf(LOG_NONE, "memory usage, or details of a single page.\n");
		return KDBG_OK;
	} else if(argc != 1 && argc != 2) {
		kprintf(LOG_NONE, "Incorrect number of arguments. See 'help %s' for help.\n", argv[0]);
		return KDBG_FAIL;
	}

	if(argc == 2) {
		if(kdbg_parse_expression(argv[1], &addr, NULL) != KDBG_OK) {
			return KDBG_FAIL;
		} else if(addr % PAGE_SIZE) {
			kprintf(LOG_NONE, "Address must be page aligned\n");
			return KDBG_FAIL;
		} else if(!(page = vm_page_lookup(addr))) {
			kprintf(LOG_NONE, "404 Page Not Found\n");
			return KDBG_FAIL;
		}

		/* Work out the ID of the queue the page is on. */
		if(page->queue) {
			queue = (int)(((ptr_t)page->queue - (ptr_t)page_queues) / sizeof(page_queue_t));
		}

		kprintf(LOG_NONE, "Page 0x%" PRIpp " (%p)\n", page->addr, page);
		kprintf(LOG_NONE, "=================================================\n");
		kprintf(LOG_NONE, "Queue:    %d (%p)\n", queue, page->queue);
		kprintf(LOG_NONE, "Modified: %d\n", page->modified);
		kprintf(LOG_NONE, "Count:    %d\n", page->count);
		kprintf(LOG_NONE, "Object:   %p\n", page->object);
		if(page->object) {
			kprintf(LOG_NONE, " Type: %p (%d)\n", page->object->type, page->object->type->id);
		}
		kprintf(LOG_NONE, "Amap:     %p\n", page->amap);
		kprintf(LOG_NONE, "Offset:   %" PRId64 "\n", page->offset);
	} else {
		kprintf(LOG_NONE, "Start              End                Pages\n");
		kprintf(LOG_NONE, "=====              ===                =====\n");

		for(i = 0; i < page_range_count; i++) {
			kprintf(LOG_NONE, "0x%-16" PRIpp " 0x%-16" PRIpp " %p\n",
			        page_ranges[i].start, page_ranges[i].end,
			        page_ranges[i].pages);
		}

		page_stats_get(&stats);
		kprintf(LOG_NONE, "\nUsage statistics\n");
		kprintf(LOG_NONE, "================\n");
		kprintf(LOG_NONE, "Total:     %" PRIu64 " KiB\n", stats.total / 1024);
		kprintf(LOG_NONE, "Allocated: %" PRIu64 " KiB\n", stats.allocated / 1024);
		kprintf(LOG_NONE, "Modified:  %" PRIu64 " KiB\n", stats.modified / 1024);
		kprintf(LOG_NONE, "Cached:    %" PRIu64 " KiB\n", stats.cached / 1024);
		kprintf(LOG_NONE, "Free:      %" PRIu64 " KiB\n", stats.free / 1024);
	}

	return KDBG_OK;
}

/** Add a page range.
 * @param start		Start of range.
 * @param end		End of range.
 * @param type		Type of range. */
static void __init_text page_range_add(phys_ptr_t start, phys_ptr_t end, int type) {
	if(page_range_count >= KERNEL_ARGS_RANGES_MAX) {
		fatal("No free page range structures");
	}

	/* Add the range, and record it in the global range array. */
	vmem_add(&page_arena, start, end - start, MM_FATAL);
	page_ranges[page_range_count].start = start;
	page_ranges[page_range_count].end = end;
	page_ranges[page_range_count].pages = NULL;
	page_ranges[page_range_count].reclaim = (type == PHYS_MEMORY_RECLAIMABLE);
	page_range_count++;

	/* If reclaimable, allocate the range to prevent it being allocated. */
	if(type == PHYS_MEMORY_RECLAIMABLE) {
		vmem_xalloc(&page_arena, end - start, 0, 0, 0, start, end, MM_FATAL);
	}
}

/** Initialise the physical memory manager.
 * @param args		Kernel arguments. */
void __init_text page_init(kernel_args_t *args) {
	phys_ptr_t start, end;
	uint32_t i;

	/* Create the arena and populate it with detected ranges. */
	vmem_early_create(&page_arena, "page_arena", 0, 0, PAGE_SIZE, NULL, NULL,
	                  NULL, 0, VMEM_RECLAIM, MM_FATAL);
	for(i = 0; i < args->phys_range_count; i++) {
		switch(args->phys_ranges[i].type) {
		case PHYS_MEMORY_FREE:
		case PHYS_MEMORY_RECLAIMABLE:
			page_range_add(args->phys_ranges[i].start,
			               args->phys_ranges[i].end,
			               args->phys_ranges[i].type);
			break;
		default:
			/* Don't care about non-usable ranges. */
			break;
		}
	}

	/* Mark the kernel init section as reclaimable. Since the kernel is
	 * marked as allocated by the bootloader, the range will not have
	 * been added by the above loop. Therefore, the range must be added
	 * first. */
	start = ((ptr_t)__init_start - KERNEL_VIRT_BASE) + args->kernel_phys;
	end = ((ptr_t)__init_end - KERNEL_VIRT_BASE) + args->kernel_phys;
	page_range_add(start, end, PHYS_MEMORY_RECLAIMABLE);

	/* Initialise architecture paging-related things. When this returns,
	 * we should be on the kernel page map. */
	page_arch_init(args);
}

/** Set up structures for each usable page. */
void __init_text vm_page_init(void) {
	size_t i, j, count, size;
	phys_ptr_t phys;

	/* Initialise page queue structures. */
	for(i = 0; i < PAGE_QUEUE_COUNT; i++) {
		list_init(&page_queues[i].pages);
		spinlock_init(&page_queues[i].lock, "page_queue_lock");
		page_queues[i].count = 0;
	}

	/* Allocate and initialise structures for each page that we have. */
	for(i = 0; i < page_range_count; i++) {
		/* Allocate a chunk of pages to store the page structures in. */
		count = (page_ranges[i].end - page_ranges[i].start) / PAGE_SIZE;
		size = ROUND_UP(count * sizeof(vm_page_t), PAGE_SIZE);
		phys = page_alloc(size / PAGE_SIZE, MM_FATAL);

		/* Map the structures into memory and initialise them. */
		page_ranges[i].pages = page_phys_map(phys, size, MM_FATAL);
		memset(page_ranges[i].pages, 0, size);
		for(j = 0; j < count; j++) {
			list_init(&page_ranges[i].pages[j].header);
			page_ranges[i].pages[j].addr = page_ranges[i].start + (j * PAGE_SIZE);
		}
	}
}

/** Reclaim memory no longer in use after kernel initialisation. */
void __init_text page_late_init(void) {
	size_t reclaimed = 0, size, i;

	page_arch_late_init();

	for(i = 0; i < page_range_count; i++) {
		if(page_ranges[i].reclaim) {
			size = page_ranges[i].end - page_ranges[i].start;
			page_free(page_ranges[i].start, size / PAGE_SIZE);
			reclaimed += size;
		}
	}

	kprintf(LOG_NORMAL, "page: reclaimed %zu KiB of unneeded memory\n", reclaimed / 1024);
}
