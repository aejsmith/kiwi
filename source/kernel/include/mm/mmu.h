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
 * @brief               MMU interface.
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

#pragma once

#include <arch/mmu.h>
#include <arch/page.h>

#include <kernel/vm.h>

#include <mm/page.h>

#include <sync/mutex.h>

/** Structure containing an MMU context. */
typedef struct mmu_context {
    mutex_t lock;                       /**< Lock to protect context. */
    arch_mmu_context_t arch;            /**< Architecture implementation details. */
} mmu_context_t;

/** MMU mapping flags. */
enum {
    /**
     * Access flags.
     */

    /** Mask to select the access flags. */
    MMU_ACCESS_MASK         = (7<<0),

    MMU_ACCESS_READ         = (1<<0),   /**< Mapping should be readable. */
    MMU_ACCESS_WRITE        = (1<<1),   /**< Mapping should be writable. */
    MMU_ACCESS_EXECUTE      = (1<<2),   /**< Mapping should be executable. */

    /**
     * Caching behaviour flags.
     */

    /** Mask to select the caching behaviour flag. */
    MMU_CACHE_MASK          = (3<<3),

    /**
     * Treat the mapping as normal memory (fully cached). The value of this
     * flag is 0 so can be omitted.
     */
    MMU_CACHE_NORMAL        = (0<<3),

    /**
     * Device memory (uncached, no reordering or combining, writes may not
     * wait for acknowledgement before completing).
     */
    MMU_CACHE_DEVICE        = (1<<3),

    /**
     * Uncached memory (uncached, no reordering or combining, writes may wait
     * for acknowledgement before completing).
     */
    MMU_CACHE_UNCACHED      = (2<<3),

    /**
     * Write-combined memory (uncached, writes can be combined into single
     * transactions). The meaning of this is somewhat architecture-specific,
     * and not all architectures support it. Where unsupported it behaves as
     * uncached.
     */
    MMU_CACHE_WRITE_COMBINE = (3<<3),
};

extern mmu_context_t kernel_mmu_context;

extern status_t arch_mmu_context_init(mmu_context_t *ctx, unsigned mmflag);
extern void arch_mmu_context_destroy(mmu_context_t *ctx);
extern status_t arch_mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    unsigned mmflag);
extern void arch_mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access);
extern bool arch_mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page);
extern bool arch_mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags);
extern void arch_mmu_context_flush(mmu_context_t *ctx);
extern void arch_mmu_context_load(mmu_context_t *ctx);
extern void arch_mmu_context_unload(mmu_context_t *ctx);

extern void mmu_context_lock(mmu_context_t *ctx);
extern void mmu_context_unlock(mmu_context_t *ctx);

extern status_t mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    unsigned mmflag);
extern void mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access);
extern bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page);
extern bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags);

extern void mmu_context_load(mmu_context_t *ctx);
extern void mmu_context_unload(mmu_context_t *ctx);

extern mmu_context_t *mmu_context_create(unsigned mmflag);
extern void mmu_context_destroy(mmu_context_t *ctx);

extern void arch_mmu_init(void);
extern void arch_mmu_init_percpu(void);

extern void mmu_init(void);
extern void mmu_init_percpu(void);
