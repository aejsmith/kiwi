/*
 * Copyright (C) 2009-2023 Alex Smith
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
 *  - Free page tables as soon as they become empty (we will retain allocated
 *    page tables until address space destruction at the moment). This needs to
 *    be synchronised properly - the page table cannot be freed until after the
 *    TLB invalidate and DSB for unsetting the TTE pointing to it. This will
 *    need to use a non-last-level TLB invalidate operation for the last page
 *    removed from the table.
 *  - 16-bit ASID support where supported by hardware.
 */

#include <arch/barrier.h>

#include <arm64/cpu.h>
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
    /* phys_map() should never fail for normal memory on ARM64. */
    return phys_map(addr, PAGE_SIZE, MM_BOOT);
}

static phys_ptr_t alloc_table(uint32_t mmflag) {
    phys_ptr_t ret;

    if (likely(page_init_done)) {
        page_t *page = page_alloc(mmflag | MM_ZERO);
        ret = (page) ? page->addr : 0;
    } else {
        ret = page_early_alloc();
        memset(map_table(ret), 0, PAGE_SIZE);
    }

    /* Ensure writes to zero the page have completed before making it visible
     * to the translation table walker. */
    if (ret)
        write_barrier();

    return ret;
}

/** Get the level 1 table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ttl1(mmu_context_t *ctx, ptr_t virt, bool alloc, uint32_t mmflag) {
    uint64_t *ttl0 = map_table(ctx->arch.ttl0);

    unsigned ttl0e = (virt / ARM64_TTL1_RANGE) % 512;
    if (!(ttl0[ttl0e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = alloc_table(mmflag);
        ttl0[ttl0e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    assert(ttl0[ttl0e] & ARM64_TTE_TABLE);
    return map_table(ttl0[ttl0e] & ARM64_TTE_ADDR_MASK);
}

/** Get the level 2 table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ttl2(mmu_context_t *ctx, ptr_t virt, bool alloc, uint32_t mmflag) {
    uint64_t *ttl1 = get_ttl1(ctx, virt, alloc, mmflag);
    if (!ttl1)
        return NULL;

    unsigned ttl1e = (virt % ARM64_TTL1_RANGE) / ARM64_TTL2_RANGE;
    if (!(ttl1[ttl1e] & ARM64_TTE_PRESENT)) {
        if (!alloc)
            return NULL;

        phys_ptr_t addr = alloc_table(mmflag);
        ttl1[ttl1e] = addr | ARM64_TTE_PRESENT | ARM64_TTE_TABLE;
    }

    /* If this function is being used it should not be a large 1GB page. */
    assert(ttl1[ttl1e] & ARM64_TTE_TABLE);
    return map_table(ttl1[ttl1e] & ARM64_TTE_ADDR_MASK);
}

/** Get the level 3 table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped page table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ttl3(mmu_context_t *ctx, ptr_t virt, bool alloc, uint32_t mmflag) {
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

    /* If this function is being used it should not be a large 2MB page. */
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

/** Flushes queued TLB invalidations. */
static void flush_invalidate_queue(mmu_context_t *ctx) {
    /* IS operations apply to other CPUs as well (same Inner Shareable domain). */

    /* DSB pre-invalidate. */
    memory_barrier();

    if (ctx->arch.invalidate_count > ARCH_MMU_INVALIDATE_QUEUE_SIZE) {
        assert(!is_kernel_context(ctx));

        /* See queue_invalidate(). Invalidate the whole ASID in this case. */
        arm64_tlbi_val(aside1is, (uint64_t)ctx->arch.asid << ARM64_TLBI_ASID_SHIFT);
    } else {
        /* Since we currently don't unmap PTEs except on un*/
        if (is_kernel_context(ctx)) {
            for (size_t i = 0; i < ctx->arch.invalidate_count; i++) {
                ptr_t addr = ctx->arch.invalidate_queue[i];
                ptr_t val  = (addr >> 12) & ARM64_TLBI_VADDR_MASK;
                arm64_tlbi_val(vaale1is, val);
            }
        } else {
            for (size_t i = 0; i < ctx->arch.invalidate_count; i++) {
                ptr_t addr = ctx->arch.invalidate_queue[i];
                ptr_t val  = ((addr >> 12) & ARM64_TLBI_VADDR_MASK) | ((uint64_t)ctx->arch.asid << ARM64_TLBI_ASID_SHIFT);
                arm64_tlbi_val(vale1is, val);
            }
        }
    }

    /* DSB post-invalidate. */
    memory_barrier();

    ctx->arch.invalidate_count = 0;
}

/** Queues a TLB entry for invalidation. */
static void queue_invalidate(mmu_context_t *ctx, ptr_t virt) {
    /* See if we have enough space in the queue. */
    bool has_space = ctx->arch.invalidate_count < ARCH_MMU_INVALIDATE_QUEUE_SIZE;
    if (!has_space) {
        /*
         * For user address spaces, we just flush the whole ASID later, as a
         * result of the count overflowing below.
         *
         * For the kernel, all mappings are flagged as global, and there's no
         * operation to flush all global TLB entries. Flushing the whole TLB
         * is probably quite expensive, so instead just flush out what we've
         * got queued.
         */
        if (is_kernel_context(ctx)) {
            flush_invalidate_queue(ctx);
            has_space = true;
        }
    }

    if (has_space)
        ctx->arch.invalidate_queue[ctx->arch.invalidate_count] = virt;

    /* Increment the count regardless, as per above. */
    ctx->arch.invalidate_count++;
}

/** Initializes a new context. */
status_t arch_mmu_context_init(mmu_context_t *ctx, uint32_t mmflag) {
    ctx->arch.invalidate_count = 0;

    /* TODO: Will need to allocate ASIDs for user contexts. */ 
    fatal_todo();
}

/** Destroys a context. */
void arch_mmu_context_destroy(mmu_context_t *ctx) {
    // TODO: Flush whole ASID to ensure cached intermediate table level entries
    // are flushed from the TLB.
    fatal_todo();
}

/** Maps a page in a context. */
status_t arch_mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    uint32_t mmflag)
{
    uint64_t *ttl3 = get_ttl3(ctx, virt, true, mmflag);
    if (!ttl3)
        return STATUS_NO_MEMORY;

    unsigned ttl3e = (virt % ARM64_TTL3_RANGE) / PAGE_SIZE;
    if (unlikely(ttl3[ttl3e] & ARM64_TTE_PRESENT))
        fatal("Mapping 0x%zx which is already mapped", virt);

    ttl3[ttl3e] = phys | ARM64_TTE_PAGE | calc_tte_flags(ctx, flags);
    return STATUS_SUCCESS;
}

/** Remaps a range with different access flags. */
void arch_mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    // TODO: Might require TLB break-before-make sequence, see manual? Possibly
    // not if just changing access?
    fatal_todo();
}

/** Unmaps a page in a context. */
bool arch_mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, page_t **_page) {
    uint64_t *ttl3 = get_ttl3(ctx, virt, false, 0);
    if (!ttl3)
        return false;

    unsigned ttl3e = (virt % ARM64_TTL3_RANGE) / PAGE_SIZE;
    if (!(ttl3[ttl3e] & ARM64_TTE_PRESENT))
        return false;

    uint64_t entry = ttl3[ttl3e];
    ttl3[ttl3e] = 0;

    if (_page)
        *_page = page_lookup(entry & ARM64_TTE_ADDR_MASK);

    queue_invalidate(ctx, virt);
    return true;
}

/** Queries details about a mapping. */
bool arch_mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags) {
    fatal_todo();
}

/** Performs remote TLB invalidation. */
void arch_mmu_context_flush(mmu_context_t *ctx) {
    flush_invalidate_queue(ctx);
}

/** Switches to another MMU context. */
void arch_mmu_context_load(mmu_context_t *ctx) {
    // TODO: Configure TCR to enable/disable user page table walking, and
    // set/clear A1 bit. Anything else need changing in there for user?
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

    kprintf(LOG_NOTICE, " %s: [0x%zx,0x%zx) -> 0x%" PRIxPHYS" (0x%x)\n", name, start, end, phys, flags);
}

static void map_pmap(void) {
    /* Search for the highest physical address we have in the memory map. */
    phys_ptr_t highest_phys = 0;
    kboot_tag_foreach(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
        phys_ptr_t end = range->start + range->size;
        if (end > highest_phys)
            highest_phys = end;
    }

    /* We always map at least 8GB, and align to a 1GB boundary so that we can
     * use 1GB blocks. */
    highest_phys = round_up(max(0x200000000ul, highest_phys), ARM64_TTL2_RANGE);
    kprintf(LOG_DEBUG, "mmu: mapping physical memory up to 0x%" PRIxPHYS "\n", highest_phys);

    size_t l1_count    = round_up(highest_phys, ARM64_TTL1_RANGE) / ARM64_TTL1_RANGE;
    uint64_t tte_flags = calc_tte_flags(&kernel_mmu_context, MMU_ACCESS_RW | MMU_CACHE_NORMAL);
    phys_ptr_t phys    = 0;

    for (size_t i = 0; i < l1_count; i++) {
        uint64_t *ttl1 = get_ttl1(&kernel_mmu_context, KERNEL_PMAP_BASE + phys, true, MM_BOOT);

        size_t page_count = (i == l1_count - 1)
            ? (highest_phys % ARM64_TTL1_RANGE) / ARM64_TTL2_RANGE
            : 512;

        for (size_t j = 0; j < page_count; j++) {
            ttl1[j] = phys | tte_flags;

            phys += ARM64_TTL2_RANGE;
        }
    }
}

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
    kernel_mmu_context.arch.asid = ARM64_ASID_KERNEL;
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

    /* Map the physical map area. */
    map_pmap();
}

/** Initialize the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
    /* Initially set kernel-only TCR configuration. */
    arm64_write_sysreg(tcr_el1, ARM64_TCR_KERNEL);
    arm64_isb();

    /* Set our MAIR value. */
    arm64_write_sysreg(mair_el1, ARM64_MAIR);
    arm64_isb();

    /* Load the kernel translation tables (TTBR1 for high half of address space). */
    uint64_t ttbr0 = (uint64_t)ARM64_ASID_UNUSED << ARM64_TTBR_ASID_SHIFT;
    uint64_t ttbr1 = kernel_mmu_context.arch.ttl0 | ((uint64_t)ARM64_ASID_KERNEL << ARM64_TTBR_ASID_SHIFT);
    arm64_write_sysreg(ttbr0_el1, ttbr0);
    arm64_write_sysreg(ttbr1_el1, ttbr1);
    arm64_isb();

    /* Invalidate the TLB - things might have changed from what KBoot set up. */
    memory_barrier();
    arm64_tlbi(vmalle1);
    memory_barrier();

    // TODO: Invalidate the caches since we've changed MAIR.
}
