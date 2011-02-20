/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		Memory management functions.
 */

#ifndef __MEMORY_H
#define __MEMORY_H

#include <arch/page.h>
#include <kboot.h>

/** Physical memory range types. */
#define PHYS_MEMORY_FREE	KBOOT_MEMORY_FREE
#define PHYS_MEMORY_ALLOCATED	KBOOT_MEMORY_ALLOCATED
#define PHYS_MEMORY_RECLAIMABLE	KBOOT_MEMORY_RECLAIMABLE
#define PHYS_MEMORY_RESERVED	KBOOT_MEMORY_RESERVED
#define PHYS_MEMORY_INTERNAL	4

extern void *kmalloc(size_t size);
extern void *krealloc(void *addr, size_t size);
extern void kfree(void *addr);

extern void phys_memory_add(phys_ptr_t start, phys_ptr_t end, int type);
extern void phys_memory_protect(phys_ptr_t start, phys_ptr_t end);
extern phys_ptr_t phys_memory_alloc(phys_ptr_t size, size_t align, bool reclaim);

extern void platform_memory_detect(void);
extern void memory_init(void);
extern void memory_finalise(void);

#endif /* __MEMORY_H */
