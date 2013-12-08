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

#include <mm/kmem.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <kernel.h>

/** Macro to check if an address range is within the physical map region. */
#if KERNEL_PMAP_OFFSET > 0
# define PMAP_CONTAINS(addr, size)	\
	(addr >= KERNEL_PMAP_OFFSET && (addr + size) <= (KERNEL_PMAP_OFFSET + KERNEL_PMAP_SIZE))
#else
# define PMAP_CONTAINS(addr, size)	((addr + size) <= KERNEL_PMAP_SIZE)
#endif

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

/** Map physical memory into the kernel address space.
 * @param addr		Physical address to map.
 * @param size		Size of range to map.
 * @param mmflag	Allocation flags.
 * @return		Pointer to mapped data. */
void *phys_map(phys_ptr_t addr, size_t size, unsigned mmflag) {
	phys_ptr_t base, end;

	if(unlikely(!size))
		return NULL;

	/* Use the physical map area if the range lies within it. */
	if(PMAP_CONTAINS(addr, size))
		return (void *)(KERNEL_PMAP_BASE + (addr - KERNEL_PMAP_OFFSET));

	/* Outside the physical map area. Must instead allocate some kernel
	 * memory space and map there. */
	base = ROUND_DOWN(addr, PAGE_SIZE);
	end = ROUND_UP(addr + size, PAGE_SIZE);
	return kmem_map(base, end - base, mmflag);
}

/** Unmap memory mapped with phys_map().
 * @param addr		Address of virtual mapping.
 * @param size		Size of range.
 * @param shared	Whether the mapping was used by other CPUs. */
void phys_unmap(void *addr, size_t size, bool shared) {
	ptr_t base, end;

	/* If the range lies within the physical map area, don't need to do
	 * anything. Otherwise, unmap and free from kernel memory. */
	if((ptr_t)addr < KERNEL_PMAP_BASE || (ptr_t)addr >= (KERNEL_PMAP_BASE + KERNEL_PMAP_SIZE)) {
		base = ROUND_DOWN((ptr_t)addr, PAGE_SIZE);
		end = ROUND_UP((ptr_t)addr + size, PAGE_SIZE);
		
		kmem_unmap((void *)base, end - base, shared);
	}
}

/** Copy the contents of a page.
 * @param dest		Destination page.
 * @param source	Source page.
 * @param mmflag	Allocation flags for mapping page in memory.
 * @return		True if successful, false if unable to map pages into
 *			memory (cannot happen if MM_WAIT is specified). */
bool phys_copy(phys_ptr_t dest, phys_ptr_t source, unsigned mmflag) {
	void *mdest, *msrc;

	assert(!(dest % PAGE_SIZE));
	assert(!(source % PAGE_SIZE));

	preempt_disable();

	mdest = phys_map(dest, PAGE_SIZE, mmflag);
	if(unlikely(!mdest)) {
		preempt_enable();
		return false;
	}

	msrc = phys_map(source, PAGE_SIZE, mmflag);
	if(unlikely(!msrc)) {
		phys_unmap(mdest, PAGE_SIZE, false);
		preempt_enable();
		return false;
	}

	memcpy(mdest, msrc, PAGE_SIZE);
	phys_unmap(msrc, PAGE_SIZE, false);
	phys_unmap(mdest, PAGE_SIZE, false);
	preempt_enable();
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
		if(addr >= memory_types[i].start && addr < (memory_types[i].end))
			return memory_types[i].type;
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

	if(memory_types_count >= MEMORY_TYPE_RANGE_MAX)
		fatal("Too many phys_set_memory_type() calls");

	memory_types[memory_types_count].start = addr;
	memory_types[memory_types_count].end = addr + size;
	memory_types[memory_types_count].start = type;
	memory_types_count++;

	spinlock_unlock(&memory_types_lock);
}
