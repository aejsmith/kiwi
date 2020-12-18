/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               Physical memory handling functions.
 */

#include <device/device.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/kmem.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <kernel.h>

/** Structure containing a memory type range. */
typedef struct memory_type_range {
    phys_ptr_t start;               /**< Start of range. */
    phys_ptr_t end;                 /**< End of range. */
    unsigned type;                  /**< Type of the range. */
} memory_type_range_t;

/** Maximum number of memory type ranges. */
#define MEMORY_TYPE_RANGE_MAX   64

/** Memory type ranges. */
static memory_type_range_t memory_types[MEMORY_TYPE_RANGE_MAX];
static size_t memory_types_count;
static SPINLOCK_DEFINE(memory_types_lock);

static inline bool pmap_contains(phys_ptr_t addr, size_t size) {
    #if KERNEL_PMAP_OFFSET > 0
        if (addr < KERNEL_PMAP_OFFSET)
            return false;
    #endif

    return ((addr + size) <= (KERNEL_PMAP_OFFSET + KERNEL_PMAP_SIZE));
}

/** Maps physical memory into the kernel address space.
 * @param addr          Physical address to map.
 * @param size          Size of range to map.
 * @param mmflag        Allocation flags.
 * @return              Pointer to mapped data. */
void *phys_map(phys_ptr_t addr, size_t size, unsigned mmflag) {
    if (unlikely(!size))
        return NULL;

    /* Use the physical map area if the range lies within it. */
    if (pmap_contains(addr, size))
        return (void *)(KERNEL_PMAP_BASE + (addr - KERNEL_PMAP_OFFSET));

    /* Outside the physical map area. Must instead allocate some kernel memory
     * space and map there. */
    phys_ptr_t base = round_down(addr, PAGE_SIZE);
    phys_ptr_t end  = round_up(addr + size, PAGE_SIZE);

    return kmem_map(base, end - base, mmflag);
}

typedef struct device_phys_map_resource {
    void *mapping;
    size_t size;
} device_phys_map_resource_t;

static void device_phys_map_resource_release(device_t *device, void *data) {
    device_phys_map_resource_t *resource = data;

    phys_unmap(resource->mapping, resource->size, true);
}

/**
 * Maps physical memory into the kernel address space, as a device-managed
 * resource (will be unmapped when the device is destroyed).
 *
 * @see                 phys_map().
 *
 * @param device        Device to register to.
 */
void *device_phys_map(device_t *device, phys_ptr_t addr, size_t size, unsigned mmflag) {
    void *mapping = phys_map(addr, size, mmflag);

    /* We only need to manage this if we had to create a new mapping. */
    if (mapping && !pmap_contains(addr, size)) {
        device_phys_map_resource_t *resource = device_resource_alloc(
            sizeof(device_phys_map_resource_t), device_phys_map_resource_release, MM_KERNEL);

        resource->mapping = mapping;
        resource->size    = size;

        device_resource_register(device, resource);
    }

    return mapping;
}

/** Unmaps memory mapped with phys_map().
 * @param addr          Address of virtual mapping.
 * @param size          Size of range.
 * @param shared        Whether the mapping was used by other CPUs. */
void phys_unmap(void *addr, size_t size, bool shared) {
    ptr_t ptr = (ptr_t)addr;

    /* If the range lies within the physical map area, don't need to do
     * anything. Otherwise, unmap and free from kernel memory. */
    if (ptr < KERNEL_PMAP_BASE || ptr > KERNEL_PMAP_END) {
        ptr_t base = round_down(ptr, PAGE_SIZE);
        ptr_t end  = round_up(ptr + size, PAGE_SIZE);
        
        kmem_unmap((void *)base, end - base, shared);
    }
}

/** Copies the contents of a page.
 * @param dest          Destination page.
 * @param source        Source page.
 * @param mmflag        Allocation flags for mapping page in memory.
 * @return              True if successful, false if unable to map pages into
 *                      memory (cannot happen if MM_WAIT is specified). */
bool phys_copy(phys_ptr_t dest, phys_ptr_t source, unsigned mmflag) {
    assert(!(dest % PAGE_SIZE));
    assert(!(source % PAGE_SIZE));

    preempt_disable();

    void *dest_map = phys_map(dest, PAGE_SIZE, mmflag);
    if (unlikely(!dest_map)) {
        preempt_enable();
        return false;
    }

    void *source_map = phys_map(source, PAGE_SIZE, mmflag);
    if (unlikely(!source_map)) {
        phys_unmap(dest_map, PAGE_SIZE, false);
        preempt_enable();
        return false;
    }

    memcpy(dest_map, source_map, PAGE_SIZE);

    phys_unmap(source_map, PAGE_SIZE, false);
    phys_unmap(dest_map, PAGE_SIZE, false);

    preempt_enable();
    return true;
}

/** Gets the memory type of a certain physical address.
 * @param addr          Physical address to get type of.
 * @return              Type of the address. If no type has been specifically
 *                      defined, MEMORY_TYPE_NORMAL will be returned. */
unsigned phys_memory_type(phys_ptr_t addr) {
    /* We do not take the lock here: doing so would mean an additional spinlock
     * acquisition for every memory mapping operation. Instead we just take the
     * current count and iterate up to that. */
    for (size_t count = memory_types_count, i = 0; i < count; i++) {
        if (addr >= memory_types[i].start && addr < (memory_types[i].end))
            return memory_types[i].type;
    }

    return MEMORY_TYPE_NORMAL;
}

/** Sets the type of a range of physical memory.
 * @warning             Does not currently handle overlaps with previously
 *                      added ranges.
 * @param addr          Start address of of range.
 * @param size          Size of range.
 * @param type          Type to give the range. */
void phys_set_memory_type(phys_ptr_t addr, phys_size_t size, unsigned type) {
    assert(!(addr & PAGE_SIZE));
    assert(!(size & PAGE_SIZE));

    spinlock_lock(&memory_types_lock);

    if (memory_types_count >= MEMORY_TYPE_RANGE_MAX)
        fatal("Too many phys_set_memory_type() calls");

    memory_types[memory_types_count].start = addr;
    memory_types[memory_types_count].end   = addr + size;
    memory_types[memory_types_count].type  = type;

    memory_types_count++;

    spinlock_unlock(&memory_types_lock);
}
