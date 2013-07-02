/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		Physical memory management.
 */

#ifndef __MM_PHYS_H
#define __MM_PHYS_H

#include <arch/page.h>

#include <mm/mm.h>

/** Memory range types. */
#define MEMORY_TYPE_NORMAL	0	/**< Normal Memory. */
#define MEMORY_TYPE_DEVICE	1	/**< Device Memory. */
#define MEMORY_TYPE_UC		2	/**< Uncacheable. */
#define MEMORY_TYPE_WC		3	/**< Write Combining. */
#define MEMORY_TYPE_WT		4	/**< Write-through. */
#define MEMORY_TYPE_WB		5	/**< Write-back. */

extern void *phys_map(phys_ptr_t addr, size_t size, int mmflag);
extern void phys_unmap(void *addr, size_t size, bool shared);

extern status_t phys_alloc(phys_size_t size, phys_ptr_t align, phys_ptr_t boundary,
	phys_ptr_t minaddr, phys_ptr_t maxaddr, int mmflag, phys_ptr_t *basep);
extern void phys_free(phys_ptr_t base, phys_size_t size);
extern bool phys_copy(phys_ptr_t dest, phys_ptr_t source, int mmflag);

extern unsigned phys_memory_type(phys_ptr_t addr);
extern void phys_set_memory_type(phys_ptr_t addr, phys_size_t size, unsigned type);

#endif /* __MM_PHYS_H */
