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
    unsigned mmflag, dma_ptr_t *_addr);
extern void dma_free(struct device *device, dma_ptr_t addr, phys_size_t size);

extern status_t device_dma_alloc(
    struct device *device, phys_size_t size, const dma_constraints_t *constraints,
    unsigned mmflag, dma_ptr_t *_addr, void **_mapping);

extern void *dma_map(struct device *device, dma_ptr_t addr, size_t size, unsigned mmflag);
extern void *dma_map_etc(
    struct device *device, dma_ptr_t addr, size_t size, uint32_t flags,
    unsigned mmflag);
extern void dma_unmap(void *addr, size_t size);

extern void *device_dma_map(struct device *device, dma_ptr_t addr, size_t size, unsigned mmflag);
extern void *device_dma_map_etc(
    struct device *device, dma_ptr_t addr, size_t size, uint32_t flags,
    unsigned mmflag);
