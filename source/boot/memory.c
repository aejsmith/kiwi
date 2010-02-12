/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Memory management functions.
 */

#include <boot/console.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <types/list.h>

#include <assert.h>
#include <fatal.h>

/** Size of the heap (64KB). */
#define HEAP_SIZE		0x10000

/** Structure used to represent a physical memory range internally. */
typedef struct memory_range {
	list_t header;			/**< Link to range list. */
	phys_ptr_t start;		/**< Start of range. */
	phys_ptr_t end;			/**< End of range. */
	int type;			/**< Type of range. */
} memory_range_t;

extern char __start[], __end[], g_boot_stack[];

/** Statically allocated heap. */
static uint8_t g_heap[HEAP_SIZE] __aligned(PAGE_SIZE);
static size_t g_heap_next = 0;

/** List of physical memory ranges. */
static LIST_DECLARE(g_memory_ranges);

/** Allocate a memory range structure.
 * @param start		Start address.
 * @param end		End address.
 * @param type		Type of range.
 * @return		Pointer to range structure. */
static memory_range_t *memory_range_alloc(phys_ptr_t start, phys_ptr_t end, int type) {
	memory_range_t *range = kmalloc(sizeof(memory_range_t));
	list_init(&range->header);
	range->start = start;
	range->end = end;
	range->type = type;
	return range;
}

/** Merge adjacent ranges.
 * @param range		Range to merge. */
static void memory_range_merge(memory_range_t *range) {
	memory_range_t *other;

	if(g_memory_ranges.next != &range->header) {
		other = list_entry(range->header.prev, memory_range_t, header);
		if(other->end == range->start && other->type == range->type) {
			range->start = other->start;
			list_remove(&other->header);
			kfree(other);
		}
	}
	if(g_memory_ranges.prev != &range->header) {
		other = list_entry(range->header.next, memory_range_t, header);
		if(other->start == range->end && other->type == range->type) {
			range->end = other->end;
			list_remove(&other->header);
			kfree(other);
		}
	}
}

/** Dump a list of physical memory ranges. */
static void phys_memory_dump(void) {
	memory_range_t *range;

	LIST_FOREACH(&g_memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);

		dprintf(" 0x%016" PRIpp "-0x%016" PRIpp ": ", range->start, range->end);
		switch(range->type) {
		case PHYS_MEMORY_FREE:
			dprintf("Free\n");
			break;
		case PHYS_MEMORY_ALLOCATED:
			dprintf("Allocated\n");
			break;
		case PHYS_MEMORY_RECLAIMABLE:
			dprintf("Reclaimable\n");
			break;
		case PHYS_MEMORY_RESERVED:
			dprintf("Reserved\n");
			break;
		case PHYS_MEMORY_INTERNAL:
			dprintf("Internal\n");
			break;
		default:
			dprintf("???\n");
			break;
		}
	}
}

/** Add a range of physical memory.
 * @param start		Start of the range (must be page-aligned).
 * @param end		End of the range (must be page-aligned).
 * @param type		Type of the range. */
static void phys_memory_add_internal(phys_ptr_t start, phys_ptr_t end, int type) {
	memory_range_t *range, *other, *split;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));

	range = memory_range_alloc(start, end, type);

	/* Try to find where to insert the region in the list. */
	LIST_FOREACH(&g_memory_ranges, iter) {
		other = list_entry(iter, memory_range_t, header);
		if(start <= other->start) {
			list_add_before(&other->header, &range->header);
			break;
		}
	}

	/* If the range has not been added, add it now. */
	if(list_empty(&range->header)) {
		list_append(&g_memory_ranges, &range->header);
	}

	/* Check if the new range has overlapped part of the previous range. */
	if(g_memory_ranges.next != &range->header) {
		other = list_entry(range->header.prev, memory_range_t, header);
		if(range->start < other->end) {
			if(other->end > range->end) {
				/* Must split the range. */
				split = memory_range_alloc(range->end, other->end, other->type);
                               	list_add_after(&range->header, &split->header);
			}
			other->end = range->start;
		}
	}

	/* Swallow up any following ranges that the new range overlaps. */
	LIST_FOREACH_SAFE(&range->header, iter) {
		if(iter == &g_memory_ranges) {
			break;
		}

		other = list_entry(iter, memory_range_t, header);
		if(other->start >= range->end) {
			break;
		} else if(other->end > range->end) {
			/* Resize the range and finish. */
			other->start = range->end;
			break;
		} else {
			/* Completely remove the range. */
			list_remove(&other->header);
		}
	}

	/* Finally, merge the region with adjacent ranges of the same type. */
	memory_range_merge(range);
}

/** Allocate memory from the heap.
 * @note		A fatal error will be raised if heap is full.
 * @param size		Size of allocation to make.
 * @return		Address of allocation. */
void *kmalloc(size_t size) {
	uint8_t *ret = g_heap + g_heap_next;

	if((g_heap_next + size) > HEAP_SIZE) {
		fatal("Exhausted available heap space");
	}

	g_heap_next += size;
	return ret;
}

/** Free memory allocated with kfree().
 * @param addr		Address of allocation. */
void kfree(void *addr) {
	/* FIXME: Implement. */
}

/** Add a range of physical memory.
 * @param start		Start of the range (must be page-aligned).
 * @param end		End of the range (must be page-aligned).
 * @param type		Type of the range. */
void phys_memory_add(phys_ptr_t start, phys_ptr_t end, int type) {
	phys_memory_add_internal(start, end, type);
	dprintf("memory: added range 0x%" PRIpp "-0x%" PRIpp " (type: %d)\n",
	        start, end, type);
}

/** Allocate a range of physical memory.
 * @note		If allocation fails, a fatal error will be raised.
 * @param size		Size of the range (multiple of the page size).
 * @param align		Alignment of the range (multiple of the page size).
 * @param reclaim	Whether to mark the range as reclaimable.
 * @return		Address of allocation. */
phys_ptr_t phys_memory_alloc(phys_ptr_t size, size_t align, bool reclaim) {
	int type = (reclaim) ? PHYS_MEMORY_RECLAIMABLE : PHYS_MEMORY_ALLOCATED;
	memory_range_t *range;
	phys_ptr_t start;

	assert(!(size % PAGE_SIZE));

	/* Find a free range that is large enough to hold the new range. */
	LIST_FOREACH(&g_memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);
		if(range->type != PHYS_MEMORY_FREE) {
			continue;
		}

		/* Align the base address and check that the range fits. */
		start = ROUND_UP(range->start, align);
		if((start + size) > range->end) {
			continue;
		}

		phys_memory_add_internal(start, start + size, type);
		dprintf("memory: allocated 0x%" PRIpp "-0x%" PRIpp " (align: 0x%zx, reclaim: %d)\n",
		        start, start + size, align, reclaim);
		return start;
	}

	/* Nothing available in all physical ranges, give an error. */
	fatal("You do not have enough memory available");
}

/** Initialise the memory manager. */
void memory_init() {
	/* Detect memory ranges. */
	platform_memory_detect();

	/* Mark the bootloader itself as internal so that it gets reclaimed
	 * before entering the kernel, and mark the heap as reclaimable so the
	 * kernel can get rid of it once it has finished with the arguments. */
	phys_memory_add(ROUND_DOWN((phys_ptr_t)((ptr_t)__start), PAGE_SIZE),
	                (phys_ptr_t)((ptr_t)__end),
	                PHYS_MEMORY_INTERNAL);
	phys_memory_add((ptr_t)g_heap, (ptr_t)g_heap + HEAP_SIZE, PHYS_MEMORY_RECLAIMABLE);

	/* Mark the boot CPU's stack as reclaimable. */
	phys_memory_add((ptr_t)g_boot_stack, (ptr_t)g_boot_stack + PAGE_SIZE, PHYS_MEMORY_RECLAIMABLE);
}

/** Reclaim internal memory and write the memory map. */
void memory_finalise() {
	memory_range_t *range;
	size_t i = 0;

	/* Reclaim all internal memory ranges. */
	LIST_FOREACH(&g_memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);
		if(range->type == PHYS_MEMORY_INTERNAL) {
			range->type = PHYS_MEMORY_FREE;
			memory_range_merge(range);
		}
	}

	/* Dump the memory map to the debug console. */
	dprintf("memory: final memory map:\n");
	phys_memory_dump();

	/* Write out all ranges to the arguments structure. */
	LIST_FOREACH(&g_memory_ranges, iter) {
		range = list_entry(iter, memory_range_t, header);

		if(i >= KERNEL_ARGS_RANGES_MAX) {
			fatal("Too many physical memory ranges");
		}

		g_kernel_args->phys_ranges[i].type = range->type;
		g_kernel_args->phys_ranges[i].start = range->start;
		g_kernel_args->phys_ranges[i].end = range->end;
		i++;
	}

	g_kernel_args->phys_range_count = i;
}
