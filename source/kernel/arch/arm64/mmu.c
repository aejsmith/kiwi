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
 *
 * TODO:
 *  - Accessed and dirty bit management. Hardware-based implementation needs
 *    atomic TTE updates like AMD64, but hardware support may not be there.
 */

#include <arm64/mmu.h>

#include <lib/string.h>

#include <mm/aspace.h>
#include <mm/mmu.h>
#include <mm/page.h>
#include <mm/phys.h>

#include <proc/thread.h>

#include <assert.h>
#include <cpu.h>
#include <kboot.h>
#include <kernel.h>
#include <smp.h>
#include <status.h>

KBOOT_LOAD(0, LARGE_PAGE_SIZE, LARGE_PAGE_SIZE, KERNEL_KMEM_BASE, KERNEL_KMEM_SIZE);

/*
 * Map in 8GB initially, arch_mmu_init() will map all available RAM. We only
 * use the physical map area for cached phys_map() mappings, therefore we can
 * set it as cached here.
 */
KBOOT_MAPPING(KERNEL_PMAP_BASE, 0, 0x200000000, KBOOT_CACHE_DEFAULT);

static inline bool is_kernel_context(mmu_context_t *ctx) {
    return ctx == &kernel_mmu_context;
}

static uint64_t *map_table(phys_ptr_t addr) {
    /* phys_map() should never fail for normal memory on AMD64. */
    return phys_map(addr, PAGE_SIZE, MM_BOOT);
}

static phys_ptr_t alloc_table(unsigned mmflag) {
    phys_ptr_t ret;

    if (likely(page_init_done)) {
        page_t *page = page_alloc(mmflag | MM_ZERO);
        ret = (page) ? page->addr : 0;
    } else {
        ret = page_early_alloc();
        memset(map_table(ret), 0, PAGE_SIZE);
    }

    return ret;
}

/** Get the level 2 table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ttl2(mmu_context_t *ctx, ptr_t virt, bool alloc, unsigned mmflag) {
    uint64_t *ttl0 = map_table(ctx->arch.ttl0);

    unsigned ttl0e = (virt / ARM64_TTL1_RANGE) % 512;
    if (!(ttl0[ttl0e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = alloc_table(mmflag);
        ttl0[ttl0e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    uint64_t *ttl1 = map_table(ttl0[ttl0e] & ARM64_TTE_ADDR_MASK);

    unsigned ttl1e = (virt % ARM64_TTL1_RANGE) / ARM64_TTL2_RANGE;
    if (!(ttl1[ttl1e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = alloc_table(mmflag);
        ttl1[ttl1e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    return map_table(ttl1[ttl1e] & ARM64_TTE_ADDR_MASK);
}

/** Get the level 3 table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped page table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ttl3(mmu_context_t *ctx, ptr_t virt, bool alloc, unsigned mmflag) {
    uint64_t *ttl2 = get_ttl2(ctx, virt, alloc, mmflag);
    if (!ttl2)
        return NULL;

    unsigned ttl2e = (virt % ARM64_TTL2_RANGE) / ARM64_TTL3_RANGE;
    if (!(ttl2[ttl2e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = alloc_table(mmflag);
        ttl2[ttl2e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    /* If this function is being used it should not be a large page. */
    assert(ttl2[ttl2e] & ARM64_TTE_TABLE);
    return map_table(ttl2[ttl2e] & ARM64_TTE_ADDR_MASK);
}

/** Calculate translation table entry flags.
 * @param ctx           MMU context.
 * @param flags         MMU mapping flags.
 * @return              TTE flags. */
static inline uint64_t calc_tte_flags(mmu_context_t *ctx, uint32_t flags) {
    uint64_t tte_flags = ARM64_TTE_PRESENT | ARM64_TTE_AF;

    if (!is_kernel_context(ctx))
        tte_flags |= ARM64_TTE_NG;

    if (flags & MMU_ACCESS_WRITE) {
        tte_flags |= (is_kernel_context(ctx)) ? ARM64_TTE_AP_P_RW_U_NA : ARM64_TTE_AP_P_RW_U_RW;
    } else {
        tte_flags |= (is_kernel_context(ctx)) ? ARM64_TTE_AP_P_RO_U_NA : ARM64_TTE_AP_P_RO_U_RO;
    }

    if (!(flags & MMU_ACCESS_EXECUTE))
        tte_flags |= ARM64_TTE_XN;

    uint32_t cache_flag = flags & MMU_CACHE_MASK;
    switch (cache_flag) {
        case MMU_CACHE_NORMAL:
            tte_flags |=
                ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_NORMAL) |
                ARM64_TTE_SH_INNER_SHAREABLE;
            break;
        case MMU_CACHE_DEVICE:
            tte_flags |=
                ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_DEVICE) |
                ARM64_TTE_SH_OUTER_SHAREABLE;
            break;
        case MMU_CACHE_UNCACHED:
            tte_flags |=
                ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_UNCACHED) |
                ARM64_TTE_SH_OUTER_SHAREABLE;
            break;
        case MMU_CACHE_WRITE_COMBINE:
            tte_flags |=
                ARM64_TTE_ATTR_INDEX(ARM64_MAIR_INDEX_WRITE_COMBINE) |
                ARM64_TTE_SH_OUTER_SHAREABLE;
            break;
        default:
            unreachable();
    }

    return tte_flags;
}

/** Initialize a new context. */
status_t arch_mmu_context_init(mmu_context_t *ctx, unsigned mmflag) {
    fatal_todo();
}

/** Destroy a context. */
void arch_mmu_context_destroy(mmu_context_t *ctx) {
    /* TODO */
}

/** Map a page in a context. */
status_t arch_mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    unsigned mmflag)
{
    fatal_todo();
}

/** Remap a range with different access flags. */
void arch_mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    fatal_todo();
}

/** Unmap a page in a context. */
bool arch_mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page) {
    fatal_todo();
}

/** Query details about a mapping. */
bool arch_mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags) {
    fatal_todo();
}

/** Perform remote TLB invalidation. */
void arch_mmu_context_flush(mmu_context_t *ctx) {
    fatal_todo();
}

/** Switch to another MMU context. */
void arch_mmu_context_load(mmu_context_t *ctx) {
    fatal_todo();
}

/** Unloads an MMU context. */
void arch_mmu_context_unload(mmu_context_t *ctx) {
    /* Nothing happens. */
}

static void map_kernel(const char *name, ptr_t start, ptr_t end, uint32_t flags) {
    /* Get the KBoot core tag which contains the kernel physical address. */
    kboot_tag_core_t *core = kboot_tag_iterate(KBOOT_TAG_CORE, NULL);
    assert(core);

    phys_ptr_t phys = (start - KERNEL_VIRT_BASE) + core->kernel_phys;

    uint64_t tte_flags = calc_tte_flags(&kernel_mmu_context, flags);

    /* Map using large pages if possible. */
    if (!(start % LARGE_PAGE_SIZE) && !(end % LARGE_PAGE_SIZE)) {
        for (phys_ptr_t addr = start; addr < end; addr += LARGE_PAGE_SIZE) {
            phys_ptr_t addr_phys = phys + addr - start;

            uint64_t *ttl2 = get_ttl2(&kernel_mmu_context, addr, true, MM_BOOT);
            unsigned ttl2e = (addr % ARM64_TTL2_RANGE) / ARM64_TTL3_RANGE;

            ttl2[ttl2e] = addr_phys | tte_flags;
        }
    } else {
        for (phys_ptr_t addr = start; addr < end; addr += PAGE_SIZE) {
            phys_ptr_t addr_phys = phys + addr - start;

            uint64_t *ttl3 = get_ttl3(&kernel_mmu_context, addr, true, MM_BOOT);
            unsigned ttl3e = (addr % ARM64_TTL3_RANGE) / PAGE_SIZE;

            ttl3[ttl3e] = addr_phys | tte_flags | ARM64_TTE_PAGE;
        }
    }

    kprintf(LOG_NOTICE, " %s: [%p,%p) -> 0x%" PRIxPHYS" (0x%x)\n", name, start, end, phys, flags);
}

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
    kernel_mmu_context.arch.ttl0 = alloc_table(MM_BOOT);

    /* Map each section of the kernel. The linker script aligns the text and
     * data sections to 2MB boundaries to allow them to be mapped using large
     * pages. */
    kprintf(LOG_NOTICE, "mmu: mapping kernel sections:\n");
    map_kernel(
        "text",
        round_down((ptr_t)__text_seg_start, LARGE_PAGE_SIZE),
        round_up((ptr_t)__text_seg_end, LARGE_PAGE_SIZE),
        MMU_ACCESS_READ | MMU_ACCESS_EXECUTE);
    map_kernel(
        "data",
        round_down((ptr_t)__data_seg_start, LARGE_PAGE_SIZE),
        round_up((ptr_t)__data_seg_end, LARGE_PAGE_SIZE),
        MMU_ACCESS_READ | MMU_ACCESS_WRITE);
    map_kernel(
        "init",
        round_down((ptr_t)__init_seg_start, PAGE_SIZE),
        round_up((ptr_t)__init_seg_end, PAGE_SIZE),
        MMU_ACCESS_READ | MMU_ACCESS_WRITE | MMU_ACCESS_EXECUTE);

    fatal_todo();
}

/** Initialize the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
    // Set MAIR, invalidate TLB and caches (after setting kernel context?)
    fatal_todo();
}
