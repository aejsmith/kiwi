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

struct mmu_context;

/** Structure containing architecture MMU operations. */
typedef struct mmu_ops {
    /** Initialize a new context.
     * @param ctx           Context to initialize.
     * @param mmflag        Allocation behaviour flags.
     * @return              Status code describing result of the operation. */
    status_t (*init)(struct mmu_context *ctx, unsigned mmflag);

    /** Destroy a context.
     * @param ctx           Context to destroy. */
    void (*destroy)(struct mmu_context *ctx);

    /** Map a page in a context.
     * @param ctx           Context to map in.
     * @param virt          Virtual address to map.
     * @param phys          Physical address to map to.
     * @param access        Mapping access flags.
     * @param mmflag        Allocation behaviour flags.
     * @return              Status code describing result of the operation. */
    status_t (*map)(
        struct mmu_context *ctx, ptr_t virt, phys_ptr_t phys, uint32_t access,
        unsigned mmflag);

    /** Remap a range with different access flags.
     * @param ctx           Context to modify.
     * @param virt          Start of range to update.
     * @param size          Size of range to update.
     * @param access        New access flags. */
    void (*remap)(struct mmu_context *ctx, ptr_t virt, size_t size, uint32_t access);

    /** Unmap a page in a context.
     * @param ctx           Context to unmap in.
     * @param virt          Virtual address to unmap.
     * @param shared        Whether the mapping was shared across multiple
     *                      CPUs. Used as an optimisation to not perform
     *                      remote TLB invalidations if not necessary.
     * @param _page         Where to pointer to page that was unmapped. May
     *                      be set to NULL if the address was mapped to
     *                      memory that doesn't have a page_t (e.g. device
     *                      memory).
     * @return              Whether a page was mapped at the virtual address. */
    bool (*unmap)(struct mmu_context *ctx, ptr_t virt, bool shared, page_t **_page);

    /** Query details about a mapping.
     * @param ctx           Context to query.
     * @param virt          Virtual address to query.
     * @param _phys         Where to store physical address the page is mapped to.
     * @param _access       Where to store access flags for the mapping.
     * @return              Whether a page is mapped at the virtual address. */
    bool (*query)(struct mmu_context *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_access);

    /** Flush a context prior to unlocking.
     * @param ctx           Context to flush. */
    void (*flush)(struct mmu_context *ctx);

    /** Load an MMU context.
     * @param ctx           Context to load. */
    void (*load)(struct mmu_context *ctx);

    /** Unload an MMU context (optional).
     * @param ctx           Context to unload. */
    void (*unload)(struct mmu_context *ctx);
} mmu_ops_t;

/** Structure containing an MMU context. */
typedef struct mmu_context {
    mutex_t lock;                   /**< Lock to protect context. */
    arch_mmu_context_t arch;        /**< Architecture implementation details. */
} mmu_context_t;

extern mmu_context_t kernel_mmu_context;
extern mmu_ops_t *mmu_ops;

extern void mmu_context_lock(mmu_context_t *ctx);
extern void mmu_context_unlock(mmu_context_t *ctx);

extern status_t mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t access,
    unsigned mmflag);
extern void mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access);
extern bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page);
extern bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_access);

extern void mmu_context_load(mmu_context_t *ctx);
extern void mmu_context_unload(mmu_context_t *ctx);

extern mmu_context_t *mmu_context_create(unsigned mmflag);
extern void mmu_context_destroy(mmu_context_t *ctx);

extern void arch_mmu_init(void);
extern void arch_mmu_init_percpu(void);

extern void mmu_init(void);
extern void mmu_init_percpu(void);
