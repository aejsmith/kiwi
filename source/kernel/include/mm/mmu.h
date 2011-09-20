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
 * @brief		MMU interface.
 *
 * General guide to MMU context usage:
 *  - Lock the context with mmu_context_lock().
 *  - Perform one or more modifications.
 *  - Unlock the context with mmu_context_unlock().
 *
 * Locking must be performed explicitly so that a lock/unlock does not need to
 * be performed many times when doing many operations at once. It also allows
 * the architecture to perform optimisations at unlock, such as queuing up
 * remote TLB invalidations and performing them all in one go.
 */

#ifndef __MM_MMU_H
#define __MM_MMU_H

#include <arch/page.h>

#include <mm/flags.h>

#include <types.h>

/** Type holding an MMU context.
 * @note		Definition is not required by generic code, it is
 *			private to the architecture. */
typedef struct mmu_context mmu_context_t;

/** Kernel MMU context. */
extern mmu_context_t kernel_mmu_context;

extern void mmu_context_lock(mmu_context_t *ctx);
extern void mmu_context_unlock(mmu_context_t *ctx);

extern status_t mmu_context_map(mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys,
                                bool write, bool execute, int mmflag);
extern void mmu_context_protect(mmu_context_t *ctx, ptr_t virt, bool write, bool execute);
extern bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, phys_ptr_t *physp);
extern bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *physp,
                              bool *writep, bool *executep);

extern void mmu_context_switch(mmu_context_t *ctx);

extern mmu_context_t *mmu_context_create(int mmflag);
extern void mmu_context_destroy(mmu_context_t *map);

extern void arch_mmu_init(void);
extern void arch_mmu_init_percpu(void);

extern void mmu_init(void);
extern void mmu_init_percpu(void);

#endif /* __MM_MMU_H */
