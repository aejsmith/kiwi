/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Kernel heap manager.
 */

#ifndef __MM_KHEAP_H
#define __MM_KHEAP_H

#include <arch/page.h>

#include <types.h>
#include <vmem.h>

extern vmem_t kheap_raw_arena;
extern vmem_t kheap_va_arena;
extern vmem_t kheap_arena;

extern status_t kheap_anon_import(vmem_resource_t base, vmem_resource_t size, int vmflag);
extern void kheap_anon_release(vmem_resource_t addr, vmem_resource_t size);

extern void *kheap_alloc(size_t size, int vmflag);
extern void kheap_free(void *addr, size_t size);

extern void *kheap_map_range(phys_ptr_t base, size_t size, int vmflag);
extern void kheap_unmap_range(void *addr, size_t size, bool shared);

extern void kheap_early_init(void);
extern void kheap_init(void);

#endif /* __MM_KHEAP_H */
