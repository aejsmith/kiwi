/*
 * Copyright (C) 2009-2023 Alex Smith
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

/**
 * Maps physical memory into the kernel address space. If possible it will use
 * a pre-existing mapping of the memory in the physical map area.
 *
 * This function is a shorthand which maps the memory as
 * (MMU_ACCESS_RW | MMU_CACHE_NORMAL). Use phys_map_etc() if other flags are
 * needed - this function should be avoided for mapping device memory, as
 * MMU_CACHE_NORMAL is likely inappropriate.
 *
 * @param addr          Physical address to map.
 * @param size          Size of range to map.
 * @param mmflag        Allocation flags.
 *
 * @return              Pointer to mapped data, or NULL on failure.
 */
void *phys_map(phys_ptr_t addr, size_t size, uint32_t mmflag) {
    return phys_map_etc(addr, size, PMAP_MMU_FLAGS, mmflag);
}

static inline bool pmap_accessible(phys_ptr_t addr, size_t size, uint32_t flags) {
    /* The physical map area is mapped as PMAP_MMU_FLAGS, can't use it for
     * anything else. */
    if (flags != PMAP_MMU_FLAGS)
        return false;

    #if KERNEL_PMAP_OFFSET > 0
        if (addr < KERNEL_PMAP_OFFSET)
            return false;
    #endif

    return ((addr + size) <= (KERNEL_PMAP_OFFSET + KERNEL_PMAP_SIZE));
}

/**
 * Maps physical memory into the kernel address space. If possible it will use
 * a pre-existing mapping of the memory in the physical map area (flags must
 * be PMAP_MMU_FLAGS to do so).
 *
 * @param addr          Physical address to map.
 * @param size          Size of range to map.
 * @param flags         MMU mapping flags.
 * @param mmflag        Allocation flags.
 *
 * @return              Pointer to mapped data, or NULL on failure.
 */
void *phys_map_etc(phys_ptr_t addr, size_t size, uint32_t flags, uint32_t mmflag) {
    if (unlikely(!size))
        return NULL;

    /* Use the physical map area if possible. */
    if (pmap_accessible(addr, size, flags))
        return (void *)(KERNEL_PMAP_BASE + (addr - KERNEL_PMAP_OFFSET));

    /* Otherwise allocate some kernel memory space and map there. */
    phys_ptr_t base = round_down(addr, PAGE_SIZE);
    phys_ptr_t end  = round_up(addr + size, PAGE_SIZE);

    void *mapping = kmem_map(base, end - base, flags, mmflag);
    if (!mapping)
        return NULL;

    size_t offset = addr - base;
    return (void *)((ptr_t)mapping + offset);
}

typedef struct device_phys_map_resource {
    void *mapping;
    size_t size;
} device_phys_map_resource_t;

static void device_phys_map_resource_release(device_t *device, void *data) {
    device_phys_map_resource_t *resource = data;

    phys_unmap(resource->mapping, resource->size);
}

/**
 * Maps physical memory into the kernel address space, as a device-managed
 * resource (will be unmapped when the device is destroyed).
 *
 * @see                 phys_map().
 *
 * @param device        Device to register to.
 */
void *device_phys_map(device_t *device, phys_ptr_t addr, size_t size, uint32_t mmflag) {
    return device_phys_map_etc(device, addr, size, PMAP_MMU_FLAGS, mmflag);
}

/**
 * Maps physical memory into the kernel address space, as a device-managed
 * resource (will be unmapped when the device is destroyed).
 *
 * @see                 phys_map().
 *
 * @param device        Device to register to.
 */
void *device_phys_map_etc(
    device_t *device, phys_ptr_t addr, size_t size, uint32_t flags,
    uint32_t mmflag)
{
    void *mapping = phys_map(addr, size, mmflag);

    /* We only need to manage this if we had to create a new mapping. */
    ptr_t ptr = (ptr_t)mapping;
    if (ptr && (ptr < KERNEL_PMAP_BASE || ptr > KERNEL_PMAP_END)) {
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
 * @param size          Size of range. */
void phys_unmap(void *addr, size_t size) {
    /* If the range lies within the physical map area, don't need to do
     * anything. Otherwise, unmap and free from kernel memory. */
    ptr_t ptr = (ptr_t)addr;
    if (ptr < KERNEL_PMAP_BASE || ptr > KERNEL_PMAP_END) {
        ptr_t base = round_down(ptr, PAGE_SIZE);
        ptr_t end  = round_up(ptr + size, PAGE_SIZE);
        
        kmem_unmap((void *)base, end - base);
    }
}

/** Copies the contents of a page.
 * @param dest          Destination page.
 * @param source        Source page.
 * @param mmflag        Allocation flags for mapping page in memory.
 * @return              True if successful, false if unable to map pages into
 *                      memory (cannot happen if MM_WAIT is specified). */
bool phys_copy(phys_ptr_t dest, phys_ptr_t source, uint32_t mmflag) {
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
        phys_unmap(dest_map, PAGE_SIZE);
        preempt_enable();
        return false;
    }

    memcpy(dest_map, source_map, PAGE_SIZE);

    phys_unmap(source_map, PAGE_SIZE);
    phys_unmap(dest_map, PAGE_SIZE);

    preempt_enable();
    return true;
}
