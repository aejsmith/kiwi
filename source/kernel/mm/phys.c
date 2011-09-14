/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		Physical memory handling functions.
 */

#include <arch/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/heap.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <proc/thread.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <kernel.h>

/** Structure containing a memory type range. */
typedef struct memory_type_range {
	phys_ptr_t start;		/**< Start of range. */
	phys_ptr_t end;			/**< End of range. */
	unsigned type;			/**< Type of the range. */
} memory_type_range_t;

/** Maximum number of memory type ranges. */
#define MEMORY_TYPE_RANGE_MAX		64

/** Memory type ranges. */
static memory_type_range_t memory_types[MEMORY_TYPE_RANGE_MAX];
static size_t memory_types_count = 0;
static SPINLOCK_DECLARE(memory_types_lock);

/** Copy the contents of a page.
 * @param dest		Destination page.
 * @param source	Source page.
 * @param mmflag	Allocation flags for mapping page in memory.
 * @return		True if successful, false if unable to map pages into
 *			memory (cannot happen if MM_SLEEP is specified). */
bool phys_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag) {
	void *mdest, *msrc;

	assert(!(dest % PAGE_SIZE));
	assert(!(source % PAGE_SIZE));

	thread_wire(curr_thread);

	mdest = phys_map(dest, PAGE_SIZE, mmflag);
	if(unlikely(!mdest)) {
		thread_unwire(curr_thread);
		return false;
	}

	msrc = phys_map(source, PAGE_SIZE, mmflag);
	if(unlikely(!msrc)) {
		phys_unmap(mdest, PAGE_SIZE, false);
		thread_unwire(curr_thread);
		return false;
	}

	memcpy(mdest, msrc, PAGE_SIZE);
	phys_unmap(msrc, PAGE_SIZE, false);
	phys_unmap(mdest, PAGE_SIZE, false);
	thread_unwire(curr_thread);
	return true;
}

/** Get the memory type of a certain physical address.
 * @param addr		Physical address to get type of.
 * @return		Type of the address. If no type has been specifically
 *			defined, MEMORY_TYPE_NORMAL will be returned. */
unsigned phys_memory_type(phys_ptr_t addr) {
	size_t count, i;

	/* We do not take the lock here: doing so would mean an additional
	 * spinlock acquisition for every memory mapping operation. Instead
	 * we just take the current count and iterate up to that. */
	for(count = memory_types_count, i = 0; i < count; i++) {
		if(addr >= memory_types[i].start && addr < (memory_types[i].end)) {
			return memory_types[i].type;
		}
	}

	return MEMORY_TYPE_NORMAL;
}

/** Set the type of a range of physical memory.
 * @warning		Does not currently handle overlaps with previously
 *			added ranges.
 * @param addr		Start address of of range.
 * @param size		Size of range.
 * @param type		Type to give the range. */
void phys_set_memory_type(phys_ptr_t addr, phys_size_t size, unsigned type) {
	assert(!(addr & PAGE_SIZE));
	assert(!(size & PAGE_SIZE));

	spinlock_lock(&memory_types_lock);

	if(memory_types_count >= MEMORY_TYPE_RANGE_MAX) {
		fatal("Too many phys_set_memory_type() calls");
	}

	memory_types[memory_types_count].start = addr;
	memory_types[memory_types_count].end = addr + size;
	memory_types[memory_types_count].start = type;
	memory_types_count++;

	spinlock_unlock(&memory_types_lock);
}
