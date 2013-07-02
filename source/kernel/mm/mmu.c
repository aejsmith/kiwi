/*
 * Copyright (C) 2011-2013 Alex Smith
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
 * @todo		ASID support.
 * @todo		Maintain an active CPU set for multicast TLB
 *			invalidation.
 */

#include <arch/memory.h>

#include <mm/malloc.h>
#include <mm/mmu.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

/** Kernel MMU context. */
mmu_context_t kernel_mmu_context;

/** Architecture defined MMU context operations. */
mmu_context_ops_t *mmu_context_ops = NULL;

/**
 * Lock an MMU context.
 *
 * Locks the specified MMU context. This must be done before performing any
 * operations on it, and the context must be unlocked with mmu_context_unlock()
 * after operations have been performed. Locks can be nested (implemented using
 * a recursive mutex).
 *
 * @param ctx		Context to lock.
 */
void mmu_context_lock(mmu_context_t *ctx) {
	thread_wire(curr_thread);
	mutex_lock(&ctx->lock);
}

/** Unlock an MMU context.
 * @param ctx		Context to unlock. */
void mmu_context_unlock(mmu_context_t *ctx) {
	/* If the lock is being released (recursion count currently 1), flush
	 * changes to the context. */
	if(mutex_recursion(&ctx->lock) == 1)
		mmu_context_ops->flush(ctx);

	mutex_unlock(&ctx->lock);
	thread_unwire(curr_thread);
}

/** Create a mapping in an MMU context.
 * @param ctx		Context to map in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to.
 * @param protect	Mapping protection flags.
 * @param mmflag	Allocation behaviour flags.
 * @return		Status code describing the result of the operation. */
status_t mmu_context_map(mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys,
	unsigned protect, int mmflag)
{
	assert(mutex_held(&ctx->lock));
	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	if(ctx == &kernel_mmu_context) {
		assert(virt >= KERNEL_BASE);
	} else {
		assert(virt < USER_SIZE);
	}

	return mmu_context_ops->map(ctx, virt, phys, protect, mmflag);
}

/** Modify protection flags on a range of mappings.
 * @param ctx		Context to modify.
 * @param virt		Start of range to update.
 * @param size		Size of range to update.
 * @param protect	New protection flags. */
void mmu_context_protect(mmu_context_t *ctx, ptr_t virt, size_t size, unsigned protect) {
	assert(mutex_held(&ctx->lock));
	assert(!(virt % PAGE_SIZE));
	assert(!(size % PAGE_SIZE));

	if(ctx == &kernel_mmu_context) {
		assert(virt >= KERNEL_BASE);
	} else {
		assert(virt < USER_SIZE);
	}

	return mmu_context_ops->protect(ctx, virt, size, protect);
}

/** Unmap a page in an MMU context.
 * @param ctx		Context to unmap from.
 * @param virt		Virtual address to unmap.
 * @param shared	Whether the mapping was shared across multiple CPUs.
 *			Used as an optimisation to not perform remote TLB
 *			invalidations if not necessary.
 * @param physp		Where to store physical address of mapping.
 * @return		Whether a page was mapped at the virtual address. */
bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, phys_ptr_t *physp) {
	assert(mutex_held(&ctx->lock));
	assert(!(virt % PAGE_SIZE));

	if(ctx == &kernel_mmu_context) {
		assert(virt >= KERNEL_BASE);
	} else {
		assert(virt < USER_SIZE);
	}

	return mmu_context_ops->unmap(ctx, virt, shared, physp);
}

/** Query details about a mapping.
 * @param ctx		Context to query.
 * @param virt		Virtual address to query.
 * @param physp		Where to store physical address the page is mapped to.
 * @param protectp	Where to store protection flags for the mapping.
 * @return		Whether a page is mapped at the virtual address. */
bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *physp, unsigned *protectp) {
	bool ret;

	assert(mutex_held(&ctx->lock));
	assert(!(virt % PAGE_SIZE));

	/* We allow checks on any address here, so that you can query a kernel
	 * address even when you are on a user address space. However, we must
	 * ensure the kernel context is locked if querying a kernel address. */
	if(virt >= KERNEL_BASE && ctx != &kernel_mmu_context) {
		mmu_context_lock(&kernel_mmu_context);
		ret = mmu_context_ops->query(&kernel_mmu_context, virt, physp, protectp);
		mmu_context_unlock(&kernel_mmu_context);
	} else {
		ret = mmu_context_ops->query(ctx, virt, physp, protectp);
	}

	return ret;
}

/**
 * Load a new MMU context.
 *
 * Switches to a new MMU context. The previously active context must first be
 * unloaded with mmu_context_unload(). This function must be called with
 * interrupts disabled.
 *
 * @param ctx		Context to load.
 */
void mmu_context_load(mmu_context_t *ctx) {
	assert(!local_irq_state());

	mmu_context_ops->load(ctx);
}

/** Unload an MMU context.
 * @param ctx		Context to unload. */
void mmu_context_unload(mmu_context_t *ctx) {
	assert(!local_irq_state());

	if(mmu_context_ops->unload)
		mmu_context_ops->unload(ctx);
}

/** Create and initialize an MMU context.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to new context, NULL on allocation failure. */
mmu_context_t *mmu_context_create(int mmflag) {
	mmu_context_t *ctx;
	status_t ret;

	ctx = kmalloc(sizeof(*ctx), mmflag);
	if(!ctx)
		return NULL;

	mutex_init(&ctx->lock, "mmu_context_lock", MUTEX_RECURSIVE);

	ret = mmu_context_ops->init(ctx, mmflag);
	if(ret != STATUS_SUCCESS) {
		kfree(ctx);
		return NULL;
	}

	return ctx;
}

/** Destroy an MMU context.
 * @param ctx		Context to destroy. */
void mmu_context_destroy(mmu_context_t *ctx) {
	mmu_context_ops->destroy(ctx);
	kfree(ctx);
}

/** Initialize the kernel MMU context. */
__init_text void mmu_init(void) {
	ptr_t end, i;

	/* Initialize the kernel context. */
	mutex_init(&kernel_mmu_context.lock, "mmu_context_lock", MUTEX_RECURSIVE);
	arch_mmu_init();

	mmu_context_lock(&kernel_mmu_context);

	/* Duplicate all virtual memory mappings created by KBoot. */
	KBOOT_ITERATE(KBOOT_TAG_VMEM, kboot_tag_vmem_t, range) {
		end = range->start + range->size;

		/* Only want to map ranges in kmem space, and non-special
		 * mappings. */
		if(range->start < KERNEL_KMEM_BASE || end > KERNEL_KMEM_BASE + KERNEL_KMEM_SIZE) {
			continue;
		} else if(range->phys == ~((uint64_t)0)) {
			continue;
		}

		for(i = 0; i < range->size; i += PAGE_SIZE) {
			mmu_context_map(&kernel_mmu_context, range->start + i,
				range->phys + i, MMU_MAP_WRITE, MM_BOOT);
		}
	}

	mmu_context_unlock(&kernel_mmu_context);

	/* Switch the boot CPU to the kernel context. */
	mmu_init_percpu();
}

/** Perform per-CPU MMU initialization. */
__init_text void mmu_init_percpu(void) {
	arch_mmu_init_percpu();

	/* Switch to the kernel context. */
	mmu_context_load(&kernel_mmu_context);
}
