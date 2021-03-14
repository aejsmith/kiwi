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
 * @brief               AMD64 MMU context implementation.
 *
 * TODO:
 *  - Proper large page support, and 1GB pages for the physical map.
 *  - PCID (ASID) support.
 */

#include <arch/barrier.h>

#include <x86/cpu.h>
#include <x86/mmu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/malloc.h>
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

/* Align the kernel to 16MB to avoid ISA DMA region. */
KBOOT_LOAD(0, 0x1000000, 0x200000, KERNEL_KMEM_BASE, KERNEL_KMEM_SIZE);

/* Map in 8GB initially, arch_mmu_init() will map all available RAM. */
KBOOT_MAPPING(KERNEL_PMAP_BASE, 0, 0x200000000, KBOOT_CACHE_DEFAULT);

/** Table mapping memory types to page table flags. */
static uint64_t memory_type_flags[] = {
    /** Normal Memory - Standard behaviour. */
    [MEMORY_TYPE_NORMAL] = 0,

    /** Device Memory - Assume MTRRs are set up correctly. */
    [MEMORY_TYPE_DEVICE] = 0,

    /** Uncacheable. */
    [MEMORY_TYPE_UC] = X86_PTE_PCD,

    /** Write Combining - PAT configured for WC if these both set. */
    [MEMORY_TYPE_WC] = X86_PTE_PCD | X86_PTE_PWT,

    /** Write-through. */
    [MEMORY_TYPE_WT] = X86_PTE_PWT,

    /** Write-back - Standard behaviour. */
    [MEMORY_TYPE_WB] = 0,
};

/** Check if a context is the kernel context. */
static inline bool is_kernel_context(mmu_context_t *ctx) {
    return ctx == &kernel_mmu_context;
}

/** Check if a context is the current context. */
static inline bool is_current_context(mmu_context_t *ctx) {
    return is_kernel_context(ctx) || (curr_cpu->aspace && ctx == curr_cpu->aspace->mmu);
}

/** Get the flags to map a PDP/page directory/page table with.
 * @param phys          Physical address of page table.
 * @return              Calculated page table entry. */
static inline uint64_t calc_table_pte(mmu_context_t *ctx, phys_ptr_t phys) {
    uint64_t entry = phys | X86_PTE_PRESENT | X86_PTE_WRITE;

    if (!is_kernel_context(ctx))
        entry |= X86_PTE_USER;

    return entry;
}

/** Calculate a PTE for a page mapping.
 * @param ctx           Context being mapped in.
 * @param phys          Physical address being mapped.
 * @param access        Access flags.
 * @return              Flags to map page with. */
static inline uint64_t calc_page_pte(mmu_context_t *ctx, phys_ptr_t phys, uint32_t access) {
    uint64_t entry = phys | X86_PTE_PRESENT;
    if (access & VM_ACCESS_WRITE)
        entry |= X86_PTE_WRITE;
    if (!(access & VM_ACCESS_EXECUTE) && cpu_features.xd)
        entry |= X86_PTE_NOEXEC;
    if (is_kernel_context(ctx)) {
        entry |= X86_PTE_GLOBAL;
    } else {
        entry |= X86_PTE_USER;
    }

    /* Get the memory type of the address and set flags accordingly. */
    entry |= memory_type_flags[phys_memory_type(phys)];

    return entry;
}

static inline void set_pte(uint64_t *pte, uint64_t val) {
    *(volatile uint64_t *)pte = val;
}

/** Clear a page table entry.
 * @param pte       Page table entry to clear.
 * @return          Previous value of the PTE. */
static inline uint64_t clear_pte(uint64_t *pte) {
    /* We must atomically swap the PTE in order to accurately get the old value
     * so we can get the accessed/dirty bits. A non-atomic update could allow a
     * CPU to access the page between reading and clearing the PTE and lose the
     * accessed/dirty bit updates. */
    return atomic_exchange((atomic_uint64_t *)pte, 0);
}

/** Test and set a page table entry.
 * @param pte           Page table entry to update.
 * @param cmp           Value to compare with.
 * @param val           Value to set if equal.
 * @return              Whether successful */
static inline bool test_and_set_pte(uint64_t *pte, uint64_t cmp, uint64_t val) {
    /* With the same reasoning as clear_pte(), this function allows safe changes
     * to page table entries to avoid accessed/dirty bit updates being lost. */
    return atomic_compare_exchange_strong((atomic_uint64_t *)pte, &cmp, val);
}

static uint64_t *map_structure(phys_ptr_t addr) {
    /* Our phys_map() implementation never fails. */
    return phys_map(addr, PAGE_SIZE, MM_BOOT);
}

static phys_ptr_t alloc_structure(unsigned mmflag) {
    page_t *page;
    phys_ptr_t ret;

    if (page_init_done) {
        page = page_alloc(mmflag | MM_ZERO);
        return (page) ? page->addr : 0;
    } else {
        ret = page_early_alloc();
        memset(map_structure(ret), 0, PAGE_SIZE);
        return ret;
    }
}

/** Get the page directory containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped page directory, NULL if not found or
 *                      on allocation failure. */
static uint64_t *get_pdir(mmu_context_t *ctx, ptr_t virt, bool alloc, unsigned mmflag) {
    /* Get the virtual address of the PML4. */
    uint64_t *pml4 = map_structure(ctx->arch.pml4);

    /* Get the page directory pointer number. A PDP covers 512GB. */
    unsigned pml4e = (virt & 0x0000fffffffff000) / 0x8000000000;
    if (!(pml4[pml4e] & X86_PTE_PRESENT)) {
        /* Allocate a new PDP if required. */
        if (alloc) {
            phys_ptr_t page = alloc_structure(mmflag);
            if (unlikely(!page))
                return NULL;

            /* Map it into the PML4. */
            set_pte(&pml4[pml4e], calc_table_pte(ctx, page));
        } else {
            return NULL;
        }
    }

    /* Get the PDP from the PML4. */
    uint64_t *pdp = map_structure(pml4[pml4e] & PHYS_PAGE_MASK);

    /* Get the page directory number. A page directory covers 1GB. */
    unsigned pdpe = (virt % 0x8000000000) / 0x40000000;
    if (!(pdp[pdpe] & X86_PTE_PRESENT)) {
        /* Allocate a new page directory if required. */
        if (alloc) {
            phys_ptr_t page = alloc_structure(mmflag);
            if (unlikely(!page))
                return NULL;

            /* Map it into the PDP. */
            set_pte(&pdp[pdpe], calc_table_pte(ctx, page));
        } else {
            return NULL;
        }
    }

    /* Return the page directory address. */
    return map_structure(pdp[pdpe] & PHYS_PAGE_MASK);
}

/** Get the page table containing a virtual address.
 * @param ctx           Context to get from.
 * @param virt          Virtual address.
 * @param alloc         Whether new entries should be allocated if non-existant.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to mapped page table, NULL if not found or on
 *                      allocation failure. */
static uint64_t *get_ptbl(mmu_context_t *ctx, ptr_t virt, bool alloc, unsigned mmflag) {
    /* Get hold of the page directory. */
    uint64_t *pdir = get_pdir(ctx, virt, alloc, mmflag);
    if (!pdir)
        return NULL;

    /* Get the page table number. A page table covers 2MB. */
    unsigned pde = (virt % 0x40000000) / 0x200000;
    if (!(pdir[pde] & X86_PTE_PRESENT)) {
        /* Allocate a new page table if required. */
        if (alloc) {
            phys_ptr_t page = alloc_structure(mmflag);
            if (unlikely(!page))
                return NULL;

            /* Map it into the page directory. */
            set_pte(&pdir[pde], calc_table_pte(ctx, page));
        } else {
            return NULL;
        }
    }

    /* If this function is being used it should not be a large page. */
    assert(!(pdir[pde] & X86_PTE_LARGE));
    return map_structure(pdir[pde] & PHYS_PAGE_MASK);
}

/** Invalidate a TLB entry for an MMU context.
 * @param ctx           Context to invalidate for.
 * @param virt          Virtual address to invalidate.
 * @param shared        Whether the mapping was shared between multiple CPUs. */
static void invalidate_page(mmu_context_t *ctx, ptr_t virt, bool shared) {
    /* Invalidate on the current CPU if we're using this context. */
    if (is_current_context(ctx))
        x86_invlpg(virt);

    if (shared) {
        /* Record the address to invalidate on other CPUs when the context is
         * unlocked. */
        if (ctx->arch.invalidate_count < INVALIDATE_ARRAY_SIZE)
            ctx->arch.pages_to_invalidate[ctx->arch.invalidate_count] = virt;

        /* Increment the count regardless. If it is found to be greater than the
         * array size when unlocking, the entire TLB will be flushed. */
        ctx->arch.invalidate_count++;
    }
}

/** Initialize a new context.
 * @param ctx           Context to initialize.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
static status_t amd64_mmu_init(mmu_context_t *ctx, unsigned mmflag) {
    ctx->arch.invalidate_count = 0;

    ctx->arch.pml4 = alloc_structure(mmflag);
    if (!ctx->arch.pml4)
        return STATUS_NO_MEMORY;

    /* Get the kernel mappings into the new PML4. */
    uint64_t *kpml4 = map_structure(kernel_mmu_context.arch.pml4);
    uint64_t *pml4  = map_structure(ctx->arch.pml4);
    for (unsigned i = 256; i < 512; i++)
        pml4[i] = kpml4[i] & ~X86_PTE_ACCESSED;

    return STATUS_SUCCESS;
}

/** Destroy a context.
 * @param ctx           Context to destroy. */
static void amd64_mmu_destroy(mmu_context_t *ctx) {
    /* Free all structures in the bottom half of the PML4 (user memory). */
    uint64_t *pml4 = map_structure(ctx->arch.pml4);
    for (unsigned i = 0; i < 256; i++) {
        if (!(pml4[i] & X86_PTE_PRESENT))
            continue;

        uint64_t *pdp = map_structure(pml4[i] & PHYS_PAGE_MASK);
        for (unsigned j = 0; j < 512; j++) {
            if (!(pdp[j] & X86_PTE_PRESENT))
                continue;

            uint64_t *pdir = map_structure(pdp[j] & PHYS_PAGE_MASK);
            for (unsigned k = 0; k < 512; k++) {
                if (!(pdir[k] & X86_PTE_PRESENT))
                    continue;

                assert(!(pdir[k] & X86_PTE_LARGE));

                phys_free(pdir[k] & PHYS_PAGE_MASK, PAGE_SIZE);
            }

            phys_free(pdp[j] & PHYS_PAGE_MASK, PAGE_SIZE);
        }

        phys_free(pml4[i] & PHYS_PAGE_MASK, PAGE_SIZE);
    }

    phys_free(ctx->arch.pml4, PAGE_SIZE);
}

/** Map a page in a context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param access        Mapping access flags.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing result of the operation. */
static status_t amd64_mmu_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t access,
    unsigned mmflag)
{
    /* Find the page table for the entry. */
    uint64_t *ptbl = get_ptbl(ctx, virt, true, mmflag);
    if (!ptbl)
        return STATUS_NO_MEMORY;

    /* Check that the mapping doesn't already exist. */
    unsigned pte = (virt % 0x200000) / PAGE_SIZE;
    if (unlikely(ptbl[pte] & X86_PTE_PRESENT))
        fatal("Mapping %p which is already mapped", virt);

    /* Set the PTE. */
    set_pte(&ptbl[pte], calc_page_pte(ctx, phys, access));
    return STATUS_SUCCESS;
}

/** Remap a range with different access flags.
 * @param ctx           Context to modify.
 * @param virt          Start of range to update.
 * @param size          Size of range to update.
 * @param access        New access flags. */
static void amd64_mmu_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    /* Loop through each page in the range. */
    ptr_t end = virt + size - 1;
    uint64_t *ptbl = NULL;
    while (virt < end) {
        /* If this is the first address or we have crossed a 2MB boundary we
         * must look up a new page table. */
        if (!ptbl || !(virt % 0x200000)) {
            ptbl = get_ptbl(ctx, virt, false, 0);
            if (!ptbl) {
                /* No page table here, skip to the next one. */
                virt = (virt - (virt % 0x200000)) + 0x200000;
                continue;
            }
        }

        /* If the mapping doesn't exist we don't need to do anything. */
        unsigned pte = (virt % 0x200000) / PAGE_SIZE;
        if (ptbl[pte] & X86_PTE_PRESENT) {
            /* Update the entry. Do this atomically to avoid losing accessed or
             * dirty bit modifications. */
            uint64_t prev;
            while (true) {
                prev = ptbl[pte];

                uint64_t entry = (prev & X86_PTE_PROTECT_MASK);
                if (access & VM_ACCESS_WRITE)
                    entry |= X86_PTE_WRITE;
                if (!(access & VM_ACCESS_EXECUTE) && cpu_features.xd)
                    entry |= X86_PTE_NOEXEC;

                if (test_and_set_pte(&ptbl[pte], prev, entry))
                    break;
            }

            /* Clear TLB entries if necessary (see note in unmap()). */
            if (prev & X86_PTE_ACCESSED)
                invalidate_page(ctx, virt, true);
        }

        virt += PAGE_SIZE;
    }
}

/** Unmap a page in a context.
 * @param ctx           Context to unmap in.
 * @param virt          Virtual address to unmap.
 * @param shared        Whether the mapping was shared across multiple CPUs.
 *                      Used as an optimisation to not perform remote TLB
 *                      invalidations if not necessary.
 * @param _page         Where to pointer to page that was unmapped.
 * @return              Whether a page was mapped at the virtual address. */
static bool amd64_mmu_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, page_t **_page) {
    /* Find the page table for the entry. */
    uint64_t *ptbl = get_ptbl(ctx, virt, false, 0);
    if (!ptbl)
        return false;

    /* If the mapping doesn't exist we don't need to do anything. */
    unsigned pte = (virt % 0x200000) / PAGE_SIZE;
    if (!(ptbl[pte] & X86_PTE_PRESENT))
        return false;

    /* Clear the entry. */
    uint64_t entry = clear_pte(&ptbl[pte]);

    page_t *page = page_lookup(entry & PHYS_PAGE_MASK);

    /* If the entry is dirty, set the modified flag on the page. */
    if (page && entry & X86_PTE_DIRTY)
        page->modified = true;

    /* If the entry has been accessed, need to flush TLB entries. A processor
     * will not cache a translation without setting the accessed flag first
     * (Intel Vol. 3A Section 4.10.2.3 "Details of TLB Use"). */
    if (entry & X86_PTE_ACCESSED)
        invalidate_page(ctx, virt, shared);

    if (_page)
        *_page = page;

    return true;
}

/** Query details about a mapping.
 * @param ctx           Context to query.
 * @param virt          Virtual address to query.
 * @param _phys         Where to store physical address the page is mapped to.
 * @param _access       Where to store access flags for the mapping.
 * @return              Whether a page is mapped at the virtual address. */
static bool amd64_mmu_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_access) {
    uint64_t entry;
    phys_ptr_t phys;
    bool ret = false;

    /* Find the page directory for the entry. */
    uint64_t *pdir = get_pdir(ctx, virt, false, 0);
    if (pdir) {
        /* Get the page table number. A page table covers 2MB. */
        unsigned pde = (virt % 0x40000000) / 0x200000;
        if (pdir[pde] & X86_PTE_PRESENT) {
            /* Handle large pages: parts of the kernel address space may be
             * mapped with large pages, so we must be able to handle queries on
             * these parts. */
            if (pdir[pde] & X86_PTE_LARGE) {
                entry = pdir[pde];
                phys = (pdir[pde] & 0x000000fffff00000ul) + (virt % 0x200000);
                ret = true;
            } else {
                uint64_t *ptbl = map_structure(pdir[pde] & PHYS_PAGE_MASK);
                unsigned pte = (virt % 0x200000) / PAGE_SIZE;
                if (ptbl[pte] & X86_PTE_PRESENT) {
                    entry = ptbl[pte];
                    phys = ptbl[pte] & PHYS_PAGE_MASK;
                    ret = true;
                }
            }
        }
    }

    if (ret) {
        if (_phys)
            *_phys = phys;
        if (_access) {
            *_access = VM_ACCESS_READ |
                ((entry & X86_PTE_WRITE) ? VM_ACCESS_WRITE : 0) |
                ((entry & X86_PTE_NOEXEC) ? 0 : VM_ACCESS_EXECUTE);
        }
    }

    return ret;
}

static status_t tlb_invalidate_func(void *_ctx) {
    mmu_context_t *ctx = _ctx;

    /* Don't need to do anything if we aren't using the context - we may have
     * switched address space between the modifying CPU sending the interrupt
     * and us receiving it. */
    if (is_current_context(ctx)) {
        /* If the number of pages to invalidate is larger than the size of the
         * address array, perform a complete TLB flush. */
        if (ctx->arch.invalidate_count > INVALIDATE_ARRAY_SIZE) {
            /* For the kernel context, we must disable PGE and reenable it to
             * perform a complete TLB flush. */
            if (is_kernel_context(ctx)) {
                x86_write_cr4(x86_read_cr4() & ~X86_CR4_PGE);
                x86_write_cr4(x86_read_cr4() | X86_CR4_PGE);
            } else {
                x86_write_cr3(x86_read_cr3());
            }
        } else {
            for (size_t i = 0; i < ctx->arch.invalidate_count; i++)
                x86_invlpg(ctx->arch.pages_to_invalidate[i]);
        }
    }

    return STATUS_SUCCESS;
}

/** Perform remote TLB invalidation.
 * @param ctx           Context to send for. */
static void amd64_mmu_flush(mmu_context_t *ctx) {
    /* Check if anything needs to be done. */
    if (cpu_count < 2 || !ctx->arch.invalidate_count) {
        ctx->arch.invalidate_count = 0;
        return;
    }

    /* If this is the kernel context, perform changes on all other CPUs, else
     * perform it on each CPU using the map. */
    if (is_kernel_context(ctx)) {
        smp_call_broadcast(tlb_invalidate_func, ctx, 0);
    } else {
        /* TODO: Multicast. */
        list_foreach(&running_cpus, iter) {
            cpu_t *cpu = list_entry(iter, cpu_t, header);
            if (cpu == curr_cpu || !cpu->aspace || ctx != cpu->aspace->mmu)
                continue;

            /* CPU is using this address space. */
            if (smp_call_single(cpu->id, tlb_invalidate_func, ctx, 0) != STATUS_SUCCESS)
                fatal("Could not perform remote TLB invalidation");
        }
    }

    ctx->arch.invalidate_count = 0;
}

/** Switch to another MMU context.
 * @param ctx           Context to switch to. */
static void amd64_mmu_load(mmu_context_t *ctx) {
    x86_write_cr3(ctx->arch.pml4);
}

/** AMD64 MMU operations. */
static mmu_ops_t amd64_mmu_ops = {
    .init    = amd64_mmu_init,
    .destroy = amd64_mmu_destroy,
    .map     = amd64_mmu_map,
    .remap   = amd64_mmu_remap,
    .unmap   = amd64_mmu_unmap,
    .query   = amd64_mmu_query,
    .flush   = amd64_mmu_flush,
    .load    = amd64_mmu_load,
};

static void map_kernel(const char *name, ptr_t start, ptr_t end, uint32_t access) {
    /* Get the KBoot core tag which contains the kernel physical address. */
    kboot_tag_core_t *core = kboot_tag_iterate(KBOOT_TAG_CORE, NULL);
    assert(core);

    phys_ptr_t phys = (start - KERNEL_VIRT_BASE) + core->kernel_phys;

    /* Map using large pages if possible. */
    if (!(start % LARGE_PAGE_SIZE) && !(end % LARGE_PAGE_SIZE)) {
        for (phys_ptr_t i = start; i < end; i += LARGE_PAGE_SIZE) {
            uint64_t *pdir = get_pdir(&kernel_mmu_context, i, true, MM_BOOT);
            unsigned pde   = (i % 0x40000000) / LARGE_PAGE_SIZE;
            uint64_t entry = calc_page_pte(&kernel_mmu_context, phys + i - start, access) | X86_PTE_LARGE;
            set_pte(&pdir[pde], entry);
        }
    } else {
        for (phys_ptr_t i = start; i < end; i += PAGE_SIZE) {
            uint64_t *ptbl = get_ptbl(&kernel_mmu_context, i, true, MM_BOOT);
            unsigned pte   = (i % 0x200000) / PAGE_SIZE;
            uint64_t entry = calc_page_pte(&kernel_mmu_context, phys + i - start, access);
            set_pte(&ptbl[pte], entry);
        }
    }

    kprintf(LOG_NOTICE, " %s: [%p,%p) -> 0x%" PRIxPHYS" (0x%x)\n", name, start, end, phys, access);
}

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
    mmu_ops = &amd64_mmu_ops;

    /* Initialize the kernel MMU context. */
    kernel_mmu_context.arch.invalidate_count = 0;
    kernel_mmu_context.arch.pml4 = alloc_structure(MM_BOOT);

    mmu_context_lock(&kernel_mmu_context);

    /* Map each section of the kernel. The linker script aligns the text and
     * data sections to 2MB boundaries to allow them to be mapped using large
     * pages. */
    kprintf(LOG_NOTICE, "mmu: mapping kernel sections:\n");
    map_kernel(
        "text",
        round_down((ptr_t)__text_seg_start, LARGE_PAGE_SIZE),
        round_up((ptr_t)__text_seg_end, LARGE_PAGE_SIZE),
        VM_ACCESS_READ | VM_ACCESS_EXECUTE);
    map_kernel(
        "data",
        round_down((ptr_t)__data_seg_start, LARGE_PAGE_SIZE),
        round_up((ptr_t)__data_seg_end, LARGE_PAGE_SIZE),
        VM_ACCESS_READ | VM_ACCESS_WRITE);
    map_kernel(
        "init",
        round_down((ptr_t)__init_seg_start, PAGE_SIZE),
        round_up((ptr_t)__init_seg_end, PAGE_SIZE),
        VM_ACCESS_READ | VM_ACCESS_WRITE | VM_ACCESS_EXECUTE);

    /* Search for the highest physical address we have in the memory map. */
    phys_ptr_t highest_phys = 0;
    kboot_tag_foreach(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
        phys_ptr_t end = range->start + range->size;
        if (end > highest_phys)
            highest_phys = end;
    }

    /* We always map at least 8GB, and align to a 1GB boundary. */
    highest_phys = round_up(max(0x200000000ul, highest_phys), 0x40000000ul);
    kprintf(LOG_DEBUG, "mmu: mapping physical memory up to 0x%" PRIxPHYS "\n", highest_phys);

    /* Create the physical map area. */
    for (phys_ptr_t i = 0; i < highest_phys; i += 0x40000000) {
        uint64_t *pdir = get_pdir(&kernel_mmu_context, i + KERNEL_PMAP_BASE, true, MM_BOOT);
        for (phys_ptr_t j = 0; j < 0x40000000; j += LARGE_PAGE_SIZE) {
            pdir[j / LARGE_PAGE_SIZE]
                = (i + j) | X86_PTE_PRESENT | X86_PTE_WRITE | X86_PTE_GLOBAL | X86_PTE_LARGE;
        }
    }

    mmu_context_unlock(&kernel_mmu_context);
}

/** Get a PAT entry. */
#define pat_entry(e, t) ((uint64_t)t << ((e) * 8))

/** Initialize the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
    /* Enable NX/XD if supported. */
    if (cpu_features.xd)
        x86_write_msr(X86_MSR_EFER, x86_read_msr(X86_MSR_EFER) | X86_EFER_NXE);

    /* Configure the PAT. We do not use the PAT bit in the page table, as
     * conflicts with the large page bit, so we make PAT3 be WC. */
    uint64_t pat =
        pat_entry(0, 0x06) | pat_entry(1, 0x04) |
        pat_entry(2, 0x07) | pat_entry(3, 0x01) |
        pat_entry(4, 0x06) | pat_entry(5, 0x04) |
        pat_entry(6, 0x07) | pat_entry(7, 0x00);
    x86_write_msr(X86_MSR_CR_PAT, pat);
}
