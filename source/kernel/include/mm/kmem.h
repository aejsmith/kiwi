/*
 * Copyright (C) 2009-2022 Alex Smith
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
