/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Kernel virtual memory allocator.
 */

#pragma once

#include <arch/page.h>

#include <mm/mm.h>

extern ptr_t kmem_raw_alloc(size_t size, uint32_t mmflag);
extern void kmem_raw_free(ptr_t addr, size_t size);

extern void *kmem_alloc(size_t size, uint32_t mmflag);
extern void *kmem_alloc_etc(size_t size, uint32_t mmu_flags, uint32_t mmflag);
extern void kmem_free(void *addr, size_t size);

extern void *kmem_map(phys_ptr_t base, size_t size, uint32_t flags, uint32_t mmflag);
extern void kmem_unmap(void *addr, size_t size);

extern void kmem_init(void);
extern void kmem_late_init(void);
