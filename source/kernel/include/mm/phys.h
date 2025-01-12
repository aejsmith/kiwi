/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Physical memory management.
 */

#pragma once

#include <arch/page.h>

#include <mm/mmu.h>

struct device;

/**
 * Mapping flags of the physical map area, and also the flags used by
 * phys_map().
 */
#define PMAP_MMU_FLAGS  (MMU_ACCESS_RW | MMU_CACHE_NORMAL)

extern void *phys_map(phys_ptr_t addr, size_t size, uint32_t mmflag);
extern void *phys_map_etc(phys_ptr_t addr, size_t size, uint32_t flags, uint32_t mmflag);
extern void phys_unmap(void *addr, size_t size);

extern void *device_phys_map(struct device *device, phys_ptr_t addr, size_t size, uint32_t mmflag);
extern void *device_phys_map_etc(
    struct device *device, phys_ptr_t addr, size_t size, uint32_t flags,
    uint32_t mmflag);

extern status_t phys_alloc(
    phys_size_t size, phys_ptr_t align, phys_ptr_t boundary,
    phys_ptr_t min_addr, phys_ptr_t max_addr, uint32_t mmflag,
    phys_ptr_t *_base);
extern void phys_free(phys_ptr_t base, phys_size_t size);

extern status_t device_phys_alloc(
    struct device *device, phys_size_t size, phys_ptr_t align,
    phys_ptr_t boundary, phys_ptr_t min_addr, phys_ptr_t max_addr,
    uint32_t mmflag, phys_ptr_t *_base);

extern bool phys_copy(phys_ptr_t dest, phys_ptr_t source, uint32_t mmflag);
