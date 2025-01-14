/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               MMU interface.
 *
 * TODO:
 *  - ASID support.
 *  - Maintain an active CPU set for multicast TLB invalidation.
 */

#include <mm/aspace.h>
#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

/** Define to enable (very) verbose debug output. */
//#define DEBUG_MMU

#ifdef DEBUG_MMU
#   define dprintf(fmt...)  kprintf(LOG_DEBUG, fmt)
#else
#   define dprintf(fmt...)
#endif

/** Kernel MMU context. */
mmu_context_t kernel_mmu_context;

/**
 * Locks the specified MMU context. This must be done before performing any
 * operations on it, and the context must be unlocked with mmu_context_unlock()
 * after operations have been performed. Locks can be nested (implemented using
 * a recursive mutex).
 *
 * @param ctx           Context to lock.
 */
void mmu_context_lock(mmu_context_t *ctx) {
    mutex_lock(&ctx->lock);
    preempt_disable();
}

/** Unlocks an MMU context.
 * @param ctx           Context to unlock. */
void mmu_context_unlock(mmu_context_t *ctx) {
    /* If the lock is being released (recursion count currently 1), flush
     * changes to the context. */
    if (mutex_recursion(&ctx->lock) == 1)
        arch_mmu_context_flush(ctx);

    preempt_enable();
    mutex_unlock(&ctx->lock);
}

/** Creates a mapping in an MMU context.
 * @param ctx           Context to map in.
 * @param virt          Virtual address to map.
 * @param phys          Physical address to map to.
 * @param flags         Mapping flags.
 * @param mmflag        Allocation behaviour flags.
 * @return              Status code describing the result of the operation. */
status_t mmu_context_map(
    mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, uint32_t flags,
    uint32_t mmflag)
{
    assert(mutex_held(&ctx->lock));
    assert(!(virt % PAGE_SIZE));
    assert(!(phys % PAGE_SIZE));

    if (ctx == &kernel_mmu_context) {
        assert(virt >= KERNEL_BASE);
    } else {
        assert(virt < USER_SIZE);
    }

    dprintf(
        "mmu: mmu_context_map(%p, %p, 0x%" PRIxPHYS ", 0x%x, 0x%x)\n",
        ctx, virt, phys, flags, mmflag);

    return arch_mmu_context_map(ctx, virt, phys, flags, mmflag);
}

/** Remaps a range with different access flags.
 * @param ctx           Context to modify.
 * @param virt          Start of range to update.
 * @param size          Size of range to update.
 * @param access        New access flags. */
void mmu_context_remap(mmu_context_t *ctx, ptr_t virt, size_t size, uint32_t access) {
    assert(mutex_held(&ctx->lock));
    assert(!(access & MMU_CACHE_MASK));
    assert(!(virt % PAGE_SIZE));
    assert(!(size % PAGE_SIZE));

    if (ctx == &kernel_mmu_context) {
        assert(virt >= KERNEL_BASE);
    } else {
        assert(virt < USER_SIZE);
    }

    dprintf("mmu: mmu_context_remap(%p, %p, 0x%zx, 0x%x)\n", ctx, virt, size, access);

    return arch_mmu_context_remap(ctx, virt, size, access);
}

/** Unmaps a page in an MMU context.
 * @param ctx           Context to unmap from.
 * @param virt          Virtual address to unmap.
 * @param _page         Where to pointer to page that was unmapped. May be set
 *                      to NULL if the address was mapped to memory that doesn't
 *                      have a page_t (e.g. device memory).
 * @return              Whether a page was mapped at the virtual address. */
bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, page_t **_page) {
    assert(mutex_held(&ctx->lock));
    assert(!(virt % PAGE_SIZE));

    if (ctx == &kernel_mmu_context) {
        assert(virt >= KERNEL_BASE);
    } else {
        assert(virt < USER_SIZE);
    }

    dprintf("mmu: mmu_context_unmap(%p, %p)\n", ctx, virt);

    return arch_mmu_context_unmap(ctx, virt, _page);
}

/** Queries details about a mapping.
 * @param ctx           Context to query.
 * @param virt          Virtual address to query.
 * @param _phys         Where to store physical address the page is mapped to.
 * @param _flags        Where to store flags for the mapping. Note that this
 *                      can return different flags to what the mapping was
 *                      created with, depending on what the architecture
 *                      supports - it is determined based on what was stored in
 *                      the page tables upon mapping.
 * @return              Whether a page is mapped at the virtual address. */
bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *_phys, uint32_t *_flags) {
    bool ret;

    assert(mutex_held(&ctx->lock));
    assert(!(virt % PAGE_SIZE));

    /* We allow checks on any address here, so that you can query a kernel
     * address even when you are on a user address space. However, we must
     * ensure the kernel context is locked if querying a kernel address. */
    if (virt >= KERNEL_BASE && ctx != &kernel_mmu_context) {
        mmu_context_lock(&kernel_mmu_context);
        ret = arch_mmu_context_query(&kernel_mmu_context, virt, _phys, _flags);
        mmu_context_unlock(&kernel_mmu_context);
    } else {
        ret = arch_mmu_context_query(ctx, virt, _phys, _flags);
    }

    return ret;
}

/**
 * Switches to a new MMU context. This function must be called with interrupts
 * disabled.
 *
 * @param ctx           Context to load.
 * @param prev          Previous context.
 */
void mmu_context_switch(mmu_context_t *ctx, mmu_context_t *prev) {
    assert(!local_irq_state());

    arch_mmu_context_switch(ctx, prev);
}

/** Creates an MMU context.
 * @param mmflag        Allocation behaviour flags.
 * @return              Pointer to new context, NULL on allocation failure. */
mmu_context_t *mmu_context_create(uint32_t mmflag) {
    mmu_context_t *ctx = kmalloc(sizeof(*ctx), mmflag);
    if (!ctx)
        return NULL;

    mutex_init(&ctx->lock, "mmu_context_lock", MUTEX_RECURSIVE);

    status_t ret = arch_mmu_context_init(ctx, mmflag);
    if (ret != STATUS_SUCCESS) {
        kfree(ctx);
        return NULL;
    }

    return ctx;
}

/** Destroys an MMU context.
 * @param ctx           Context to destroy. */
void mmu_context_destroy(mmu_context_t *ctx) {
    arch_mmu_context_destroy(ctx);
    kfree(ctx);
}

/** Initialize the kernel MMU context. */
__init_text void mmu_init(void) {
    /* Initialize the kernel context. */
    mutex_init(&kernel_mmu_context.lock, "mmu_context_lock", MUTEX_RECURSIVE);
    arch_mmu_init();

    mmu_context_lock(&kernel_mmu_context);

    /* Duplicate all virtual memory mappings created by KBoot. */
    kboot_tag_foreach(KBOOT_TAG_VMEM, kboot_tag_vmem_t, range) {
        ptr_t end = range->start + range->size - 1;

        /* Only want to map ranges in kmem space, and non-special mappings. */
        if (range->start < KERNEL_KMEM_BASE || end > KERNEL_KMEM_END) {
            continue;
        } else if (range->phys == ~((uint64_t)0)) {
            continue;
        }

        uint32_t flags = MMU_ACCESS_RW;

        switch (range->cache) {
            case KBOOT_CACHE_UC:
                /* This is used for device mappings (e.g. UART). */
                flags |= MMU_CACHE_DEVICE;
                break;
            case KBOOT_CACHE_WT:
                /* This is used for the framebuffer. */
                flags |= MMU_CACHE_WRITE_COMBINE;
                break;
            default:
                flags |= MMU_CACHE_NORMAL;
                break;
        }

        for (ptr_t i = 0; i < range->size; i += PAGE_SIZE) {
            mmu_context_map(
                &kernel_mmu_context, range->start + i, range->phys + i,
                flags, MM_BOOT);
        }
    }

    mmu_context_unlock(&kernel_mmu_context);

    /* Switch the boot CPU to the kernel context. */
    mmu_init_percpu();
}

/** Perform late MMU initialisation needed to support userspace. */
__init_text void mmu_late_init(void) {
    arch_mmu_late_init();
}

/** Perform per-CPU MMU initialization. */
__init_text void mmu_init_percpu(void) {
    /* This should switch to the kernel context. */
    arch_mmu_init_percpu();
}
