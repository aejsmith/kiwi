/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               DMA memory API.
 *
 * TODO:
 *  - Actually implement support for DMA address space != CPU physical address
 *    space. For now this is just a placeholder wrapper around phys_*().
 *    Basically this needs some sort of translation information stored in
 *    device_t which would be derived from e.g. the FDT. We'd also need to
 *    translate the address constraints to the DMA address space when
 *    allocating.
 *  - Implement a DMA pool allocator for sub-page DMA allocations, and also for
 *    improved performance where constraints don't hit the phys_alloc() fast
 *    path. We could recycle Slab for this, forced to use external tracking
 *    structures. This could have an option to keep the memory permanently
 *    mapped for fast kernel access.
 *  - Need to handle cache coherency... we map by default in dma_map() with
 *    MMU_CACHE_NORMAL, but we would need some explicit cache operations for
 *    non-coherent devices.
 */

#include <device/device.h>
#include <device/dma.h>

#include <status.h>

static phys_ptr_t dma_to_phys(device_t *device, dma_ptr_t dma) {
    // See TODO at top.
    return (phys_ptr_t)dma;
}

static dma_ptr_t dma_from_phys(device_t *device, phys_ptr_t phys) {
    // See TODO at top.
    return (dma_ptr_t)phys;
}

/**
 * Allocates pages of physical memory suitable for DMA on the specified device
 * satisfying the given constraints.
 *
 * The returned address is specific to the DMA address space of the specified
 * device. It cannot be used with other physical memory management functions.
 * To map this memory into the kernel virtual address space, use dma_map*().
 *
 * When this memory is no longer needed, it must be freed with dma_free() using
 * the same device.
 *
 * Note that this function calls through to phys_alloc() and therefore has the
 * same behaviour regarding fast vs. slow allocations.
 *
 * @param device        Device to allocate for.
 * @param size          Size of the allocation (multiple of PAGE_SIZE).
 * @param constraints   Constraints for the allocation. If NULL, no constraints
 *                      apply (equivalent to a zero-initialized structure).
 * @param mmflag        Allocation flags.
 * @param _addr         Where to store address of allocation.
 *
 * @return              Status code describing result of the operation.
 */
status_t dma_alloc(
    device_t *device, phys_size_t size, const dma_constraints_t *constraints,
    unsigned mmflag, dma_ptr_t *_addr)
{
    // TODO: Translate constraints to physical.
    dma_constraints_t constr = {};
    if (constraints)
        constr = *constraints;

    phys_ptr_t phys;
    status_t ret = phys_alloc(size, constr.align, 0, 0, constr.max_addr, mmflag, &phys);
    if (ret == STATUS_SUCCESS)
        *_addr = dma_from_phys(device, phys);

    return ret;
}

/**
 * Frees memory previously allocated with dma_alloc(). The specified device must
 * be the same as the one the address was allocated for.
 *
 * @param device        Device that memory was allocated for.
 * @param addr          Base address of allocation.
 * @param size          Size of allocation.
 */
void dma_free(device_t *device, dma_ptr_t addr, phys_size_t size) {
    phys_free(dma_to_phys(device, addr), size);
}

/**
 * Allocates pages of physical memory suitable for DMA, as a device-managed
 * resource (will be freed when the device is destroyed).
 *
 * @see                 dma_alloc().
 */
status_t device_dma_alloc(
    device_t *device, phys_size_t size, const dma_constraints_t *constraints,
    unsigned mmflag, dma_ptr_t *_addr, void **_mapping)
{
    // TODO: Translate constraints to physical.
    dma_constraints_t constr = {};
    if (constraints)
        constr = *constraints;

    phys_ptr_t phys;
    status_t ret = device_phys_alloc(device, size, constr.align, 0, 0, constr.max_addr, mmflag, &phys);
    if (ret == STATUS_SUCCESS)
        *_addr = dma_from_phys(device, phys);

    return ret;
}

/**
 * Maps DMA memory into the kernel address space. The specified memory must
 * have been allocated for the specified device.
 *
 * As with phys_map(), this maps as (MMU_ACCESS_RW | MMU_CACHE_NORMAL). Use
 * dma_map_etc() if other flags are needed.
 *
 * @todo                Support for non-coherent devices.
 *
 * @param device        Device memory is allocated for.
 * @param addr          Address of memory to map.
 * @param size          Size of range to map.
 * @param mmflag        Allocation behaviour flags.
 *
 * @return              Pointer to mapped memory, or NULL on failure.
 */
void *dma_map(device_t *device, dma_ptr_t addr, size_t size, unsigned mmflag) {
    return phys_map(dma_to_phys(device, addr), size, mmflag);
}

/**
 * Maps DMA memory into the kernel address space. The specified memory must
 * have been allocated for the specified device.
 *
 * @todo                Support for non-coherent devices.
 *
 * @param device        Device memory is allocated for.
 * @param addr          Address of memory to map.
 * @param size          Size of range to map.
 * @param flags         MMU mapping flags.
 * @param mmflag        Allocation behaviour flags.
 *
 * @return              Pointer to mapped memory, or NULL on failure.
 */
void *dma_map_etc(
    device_t *device, dma_ptr_t addr, size_t size, uint32_t flags,
    unsigned mmflag)
{
    return phys_map_etc(dma_to_phys(device, addr), size, flags, mmflag);
}

/** Unmaps memory previously mapped with dma_map*().
 * @param addr          Memory to unmap.
 * @param size          Size of memory range. */
void dma_unmap(void *addr, size_t size) {
    return phys_unmap(addr, size, true);
}

/**
 * Maps DMA memory into the kernel address space, as a device-managed resource
 * (will be unmapped when the device is destroyed).
 *
 * @see                 dma_map().
 */
void *device_dma_map(device_t *device, dma_ptr_t addr, size_t size, unsigned mmflag) {
    return device_phys_map(device, dma_to_phys(device, addr), size, mmflag);
}

/**
 * Maps DMA memory into the kernel address space, as a device-managed resource
 * (will be unmapped when the device is destroyed).
 *
 * @see                 dma_map().
 */
void *device_dma_map_etc(
    device_t *device, dma_ptr_t addr, size_t size, uint32_t flags,
    unsigned mmflag)
{
    return device_phys_map_etc(device, dma_to_phys(device, addr), size, flags, mmflag);
}
