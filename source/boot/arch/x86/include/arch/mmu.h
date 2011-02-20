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
 * @brief		x86 MMU functions.
 */

#ifndef __ARCH_MMU_H
#define __ARCH_MMU_H

#include <types.h>

/** x86 MMU context structure. */
typedef struct mmu_context {
	phys_ptr_t cr3;			/**< Value loaded into CR3. */
	bool is64;			/**< Whether this is a 64-bit context. */
} mmu_context_t;

extern bool mmu_map(mmu_context_t *ctx, uint64_t virt, phys_ptr_t phys, uint64_t size);
extern mmu_context_t *mmu_create(bool is64);

#endif /* __ARCH_MMU_H */
