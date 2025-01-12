/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

    /** Shortcut for (MMU_ACCESS_READ | MMU_ACCESS_WRITE). */
    MMU_ACCESS_RW           = MMU_ACCESS_READ | MMU_ACCESS_WRITE,

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

extern status_t arch_mmu_context_init(mmu_context_t *ctx, uint32_t mmflag);
extern void arch_mmu_context_destroy(mmu_context_t *ctx);
extern status_t arch_mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    uint32_t mmflag);
extern void arch_mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access);
extern bool arch_mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, page_t **_page);
extern bool arch_mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags);
extern void arch_mmu_context_flush(mmu_context_t *ctx);
extern void arch_mmu_context_switch(mmu_context_t *ctx, mmu_context_t *prev);

extern void mmu_context_lock(mmu_context_t *ctx);
extern void mmu_context_unlock(mmu_context_t *ctx);

extern status_t mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    uint32_t mmflag);
extern void mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access);
extern bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, page_t **_page);
extern bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags);

extern void mmu_context_switch(mmu_context_t *ctx, mmu_context_t *prev);

extern mmu_context_t *mmu_context_create(uint32_t mmflag);
extern void mmu_context_destroy(mmu_context_t *ctx);

extern void arch_mmu_init(void);
extern void arch_mmu_late_init(void);
extern void arch_mmu_init_percpu(void);

extern void mmu_init(void);
extern void mmu_late_init(void);
extern void mmu_init_percpu(void);
