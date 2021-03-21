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
status_t arch_mmu_context_init(mmu_context_t *ctx, unsigned mmflag) {
    fatal_todo();
}

/** Destroy a context.
 * @param ctx           Context to destroy. */
void arch_mmu_context_destroy(mmu_context_t *ctx) {
    /* TODO */
}

/** Map a page in a context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param access        Mapping access flags.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
status_t arch_mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t access,
    unsigned mmflag)
{
    fatal_todo();
}

/** Remap a range with different access flags.
 * @param ctx           Context to modify.
 * @param virt          Start of range to update.
 * @param size          Size of range to update.
 * @param access        New access flags. */
void arch_mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    fatal_todo();
}

/** Unmap a page in a context.
 * @param ctx           Context to unmap in.
 * @param virt          Virtual address to unmap.
 * @param shared        Whether the mapping was shared across multiple CPUs.
 *                      Used as an optimisation to not perform remote TLB
 *                      invalidations if not necessary.
 * @param _page         Where to pointer to page that was unmapped.
 * @return              Whether a page was mapped at the virtual address. */
bool arch_mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page) {
    fatal_todo();
}

/** Query details about a mapping.
 * @param ctx           Context to query.
 * @param virt          Virtual address to query.
 * @param _phys         Where to store physical address the page is mapped to.
 * @param _access       Where to store access flags for the mapping.
 * @return              Whether a page is mapped at the virtual address. */
bool arch_mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_access) {
    fatal_todo();
}

/** Perform remote TLB invalidation.
 * @param ctx           Context to send for. */
void arch_mmu_context_flush(mmu_context_t *ctx) {
    fatal_todo();
}

/** Switch to another MMU context.
 * @param ctx           Context to switch to. */
void arch_mmu_context_load(mmu_context_t *ctx) {
    fatal_todo();
}

/** Unloads an MMU context.
 * @param ctx           Context to unload. */
void arch_mmu_context_unload(mmu_context_t *ctx) {
    /* Nothing happens. */
}

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
    fatal_todo();
}

/** Initialize the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
    fatal_todo();
}
