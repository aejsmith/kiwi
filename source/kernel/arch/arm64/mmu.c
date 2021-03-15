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
 * @brief               ARM64 MMU context implementation.
 */

#include <mm/aspace.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <kboot.h>
#include <kernel.h>
#include <smp.h>
#include <status.h>

KBOOT_LOAD(0, LARGE_PAGE_SIZE, LARGE_PAGE_SIZE, KERNEL_KMEM_BASE, KERNEL_KMEM_SIZE);

/* Map in 8GB initially, arch_mmu_init() will map all available RAM. */
// TODO: Need to deal with caching here.
//KBOOT_MAPPING(KERNEL_PMAP_BASE, 0, 0x200000000, KBOOT_CACHE_DEFAULT);

/** Initialize a new context.
 * @param ctx           Context to initialize.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
static status_t arm64_mmu_init(mmu_context_t *ctx, unsigned mmflag) {
    fatal("TODO");
}

/** Destroy a context.
 * @param ctx           Context to destroy. */
static void arm64_mmu_destroy(mmu_context_t *ctx) {
    /* TODO */
}

/** Map a page in a context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param access        Mapping access flags.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
static status_t arm64_mmu_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t access,
    unsigned mmflag)
{
    fatal("TODO");
}

/** Remap a range with different access flags.
 * @param ctx           Context to modify.
 * @param virt          Start of range to update.
 * @param size          Size of range to update.
 * @param access        New access flags. */
static void arm64_mmu_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    fatal("TODO");
}

/** Unmap a page in a context.
 * @param ctx           Context to unmap in.
 * @param virt          Virtual address to unmap.
 * @param shared        Whether the mapping was shared across multiple CPUs.
 *                      Used as an optimisation to not perform remote TLB
 *                      invalidations if not necessary.
 * @param _page         Where to pointer to page that was unmapped.
 * @return              Whether a page was mapped at the virtual address. */
static bool arm64_mmu_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page) {
    fatal("TODO");
}

/** Query details about a mapping.
 * @param ctx           Context to query.
 * @param virt          Virtual address to query.
 * @param _phys         Where to store physical address the page is mapped to.
 * @param _access       Where to store access flags for the mapping.
 * @return              Whether a page is mapped at the virtual address. */
static bool arm64_mmu_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_access) {
    fatal("TODO");
}

/** Perform remote TLB invalidation.
 * @param ctx           Context to send for. */
static void arm64_mmu_flush(mmu_context_t *ctx) {
    fatal("TODO");
}

/** Switch to another MMU context.
 * @param ctx           Context to switch to. */
static void arm64_mmu_load(mmu_context_t *ctx) {
    fatal("TODO");
}

/** ARM64 MMU operations. */
static mmu_ops_t arm64_mmu_ops = {
    .init    = arm64_mmu_init,
    .destroy = arm64_mmu_destroy,
    .map     = arm64_mmu_map,
    .remap   = arm64_mmu_remap,
    .unmap   = arm64_mmu_unmap,
    .query   = arm64_mmu_query,
    .flush   = arm64_mmu_flush,
    .load    = arm64_mmu_load,
};

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
    mmu_ops = &arm64_mmu_ops;

    fatal("TODO");
}

/** Get a PAT entry. */
#define pat_entry(e, t) ((uint64_t)t << ((e) * 8))

/** Initialize the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
    fatal("TODO");
}
