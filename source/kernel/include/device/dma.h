/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               DMA memory API.
 *
 * This API provides a facility for allocating and mapping memory suitable for
 * DMA access by devices.
 *
 * Devices may have constraints on the memory addresses that can be used for
 * DMA. Therefore the DMA memory API allows specification of constraints for
 * allocations.
 *
 * Additionally, the address space used by a device for DMA may not correspond
 * directly to the CPU physical address space. Therefore the API works with
 * device-local addresses and handles translation between CPU and device
 * address spaces.
 */

#pragma once

#include <mm/phys.h>

struct device;

/**
 * DMA address to be passed to a device. Valid only for the device that it was
 * allocated for.
 */
typedef uint64_t dma_ptr_t;

/**
 * Structure specifying a device's DMA address constraints. This can be zero-
 * initialized for no constraint. Note that address constraints are specified
 * in terms of the device's DMA address space.
 */
typedef struct dma_constraints {
    dma_ptr_t max_addr;             /**< Maximum end address of a DMA memory range. */
    dma_ptr_t align;                /**< Required alignment of DMA addresses. */
} dma_constraints_t;

/** Maximum address for 32-bit DMA devices. */
#define DMA_MAX_ADDR_32BIT  0x100000000ull

extern status_t dma_alloc(
    struct device *device, phys_size_t size, const dma_constraints_t *constraints,
    uint32_t mmflag, dma_ptr_t *_addr);
extern void dma_free(struct device *device, dma_ptr_t addr, phys_size_t size);

extern status_t device_dma_alloc(
    struct device *device, phys_size_t size, const dma_constraints_t *constraints,
    uint32_t mmflag, dma_ptr_t *_addr);

extern void *dma_map(struct device *device, dma_ptr_t addr, size_t size, uint32_t mmflag);
extern void *dma_map_etc(
    struct device *device, dma_ptr_t addr, size_t size, uint32_t flags,
    uint32_t mmflag);
extern void dma_unmap(void *addr, size_t size);

extern void *device_dma_map(struct device *device, dma_ptr_t addr, size_t size, uint32_t mmflag);
extern void *device_dma_map_etc(
    struct device *device, dma_ptr_t addr, size_t size, uint32_t flags,
    uint32_t mmflag);
