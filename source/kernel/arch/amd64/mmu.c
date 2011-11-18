/*
 * Copyright (C) 2009-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		AMD64 MMU context implementation.
 */

#include <arch/barrier.h>
#include <arch/memory.h>

#include <x86/cpu.h>
#include <x86/mmu.h>

#include <cpu/cpu.h>
#include <cpu/smp.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/phys.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <kboot.h>
#include <kernel.h>
#include <status.h>

/** Check if an MMU context is the kernel context. */
#define IS_KERNEL_CTX(ctx)	(ctx == &kernel_mmu_context)

/** Return flags to map a PDP/page directory/page table with. */
#define TABLE_MAPPING_FLAGS(ctx)	\
	(IS_KERNEL_CTX(ctx) ? (X86_PTE_PRESENT | X86_PTE_WRITE) : (X86_PTE_PRESENT | X86_PTE_WRITE | X86_PTE_USER))

/** Determine if an MMU context is the current context. */
#define IS_CURRENT_CTX(ctx)	(IS_KERNEL_CTX(ctx) || (curr_aspace && ctx == curr_aspace->mmu))

/** Validate operation arguments. */
#if CONFIG_DEBUG
# define CHECK_OPERATION(ctx, virt, phys)	\
	assert(mutex_held(&ctx->lock)); \
	assert(!(virt % PAGE_SIZE)); \
	assert(!(phys % PAGE_SIZE)); \
	if(IS_KERNEL_CTX(ctx)) { \
		assert(virt >= KERNEL_MEMORY_BASE); \
	} else { \
		assert(virt < USER_MEMORY_SIZE); \
	}
#else
# define CHECK_OPERATION(ctx, virt, phys)	
#endif

extern char __text_start[], __text_end[];
extern char __init_start[], __init_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __bss_end[];

/** Define a boot mapping of the first 8GB of physical memory. */
KBOOT_MAPPING(KERNEL_PMAP_BASE, 0, 0x200000000);

/** Table mapping memory types to page table flags. */
static struct { bool pat; uint64_t flags; } memory_type_flags[] = {
	/** Normal Memory - Standard behaviour. */
	{ false, 0 },

	/** Device Memory - Assume MTRRs are set up correctly. */
	{ false, 0 },

	/** Uncacheable. */
	{ false, X86_PTE_PCD },

	/** Write Combining - PAT configured for WC if these both set. */
	{ true, X86_PTE_PCD | X86_PTE_PWT },

	/** Write-through. */
	{ false, X86_PTE_PWT },

	/** Write-back - Standard behaviour. */
	{ false, 0 },
};

/** Kernel MMU context. */
mmu_context_t kernel_mmu_context;

/** Allocate a paging structure.
 * @param mmflag	Allocation flags.
 * @return		Address of structure on success, 0 on failure. */
static phys_ptr_t alloc_structure(int mmflag) {
	page_t *page = page_alloc(mmflag | PM_ZERO);
        return (page) ? page->addr : 0;
}

/** Get the virtual address of a page structure.
 * @param addr		Address of structure.
 * @return		Pointer to mapping. */
static uint64_t *map_structure(phys_ptr_t addr) {
	/* Our phys_map() implementation never fails. */
	return phys_map(addr, PAGE_SIZE, MM_FATAL);
}

/** Get the page directory containing a virtual address.
 * @param ctx		Context to get from.
 * @param virt		Virtual address.
 * @param alloc		Whether new entries should be allocated if non-existant.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to mapped page directory, NULL if not found or
 *			on allocation failure. */
static uint64_t *mmu_context_get_pdir(mmu_context_t *ctx, ptr_t virt, bool alloc, int mmflag) {
	uint64_t *pml4, *pdp;
	unsigned pml4e, pdpe;
	phys_ptr_t page;

	/* Get the virtual address of the PML4. */
	pml4 = map_structure(ctx->pml4);

	/* Get the page directory pointer number. A PDP covers 512GB. */
	pml4e = (virt & 0x0000FFFFFFFFF000) / 0x8000000000;
	if(!(pml4[pml4e] & X86_PTE_PRESENT)) {
		/* Allocate a new PDP if required. */
		if(alloc) {
			page = alloc_structure(mmflag);
			if(unlikely(!page)) {
				return NULL;
			}

			/* Map it into the PML4. */
			pml4[pml4e] = page | TABLE_MAPPING_FLAGS(ctx);
		} else {
			return NULL;
		}
	}

	/* Get the PDP from the PML4. */
	pdp = map_structure(pml4[pml4e] & PHYS_PAGE_MASK);

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!(pdp[pdpe] & X86_PTE_PRESENT)) {
		/* Allocate a new page directory if required. */
		if(alloc) {
			page = alloc_structure(mmflag);
			if(unlikely(!page)) {
				return NULL;
			}

			/* Map it into the PDP. */
			pdp[pdpe] = page | TABLE_MAPPING_FLAGS(ctx);
		} else {
			return NULL;
		}
	}

	/* Return the page directory address. */
	return map_structure(pdp[pdpe] & PHYS_PAGE_MASK);
}

/** Get the page table containing a virtual address.
 * @param ctx		Context to get from.
 * @param virt		Virtual address.
 * @param alloc		Whether new entries should be allocated if non-existant.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to mapped page table, NULL if not found or on
 *			allocation failure. */
static uint64_t *mmu_context_get_ptbl(mmu_context_t *ctx, ptr_t virt, bool alloc, int mmflag) {
	phys_ptr_t page;
	uint64_t *pdir;
	unsigned pde;

	/* Get hold of the page directory. */
	pdir = mmu_context_get_pdir(ctx, virt, alloc, mmflag);
	if(!pdir) {
		return NULL;
	}

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / 0x200000;
	if(!(pdir[pde] & X86_PTE_PRESENT)) {
		/* Allocate a new page table if required. */
		if(alloc) {
			page = alloc_structure(mmflag);
			if(unlikely(!page)) {
				return NULL;
			}

			/* Map it into the page directory. */
			pdir[pde] = page | TABLE_MAPPING_FLAGS(ctx);
		} else {
			return NULL;
		}
	}

	/* If this function is being used it should not be a large page. */
	assert(!(pdir[pde] & X86_PTE_LARGE));
	return map_structure(pdir[pde] & PHYS_PAGE_MASK);
}

/** Invalidate a TLB entry for an MMU context.
 * @param ctx		Context to invalidate for.
 * @param virt		Virtual address to invalidate.
 * @param shared	Whether the mapping was shared between multiple CPUs. */
static void mmu_context_invalidate(mmu_context_t *ctx, ptr_t virt, bool shared) {
	/* Invalidate on the current CPU if we're using this context. */
	if(IS_CURRENT_CTX(ctx)) {
		x86_invlpg(virt);
	}
#if CONFIG_SMP
	/* Record the address to invalidate on other CPUs when the context is
	 * unlocked. */
	if(ctx->invalidate_count < INVALIDATE_ARRAY_SIZE) {
		ctx->pages_to_invalidate[ctx->invalidate_count] = virt;
	}

	/* Increment the count regardless. If it is found to be greater than
	 * the array size when unlocking, the entire TLB will be flushed. */
	ctx->invalidate_count++;
#endif
}

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

#if CONFIG_SMP
/** Remote TLB invalidation handler.
 * @param _ctx		Address of MMU context structure.
 * @return		Always returns STATUS_SUCCESS. */
static status_t tlb_invalidate_call_func(void *_ctx) {
	mmu_context_t *ctx = _ctx;
	size_t i;

	/* Don't need to do anything if we aren't using the context - we may
	 * have switched address space between the modifying CPU sending the
	 * interrupt and us receiving it. */
	if(IS_CURRENT_CTX(ctx)) {
		/* If the number of pages to invalidate is larger than the size
		 * of the address array, perform a complete TLB flush. */
		if(ctx->invalidate_count > INVALIDATE_ARRAY_SIZE) {
			/* For the kernel context, we must disable PGE and
			 * reenable it to perform a complete TLB flush. */
			if(IS_KERNEL_CTX(ctx)) {
				x86_write_cr4(x86_read_cr4() & ~X86_CR4_PGE);
				x86_write_cr4(x86_read_cr4() | X86_CR4_PGE);
			} else {
				x86_write_cr3(x86_read_cr3());
			}
		} else {
			for(i = 0; i < ctx->invalidate_count; i++) {
				x86_invlpg(ctx->pages_to_invalidate[i]);
			}
		}
	}

	return STATUS_SUCCESS;
}

/** Perform remote TLB invalidation.
 * @param ctx		Context to send for. */
static void mmu_context_flush(mmu_context_t *ctx) {
	cpu_t *cpu;

	/* Check if anything needs to be done. */
	if(cpu_count < 2 || !ctx->invalidate_count) {
		ctx->invalidate_count = 0;
		return;
	}

	/* If this is the kernel context, perform changes on all other CPUs,
	 * else perform it on each CPU using the map. */
	if(IS_KERNEL_CTX(ctx)) {
		smp_call_broadcast(tlb_invalidate_call_func, ctx, 0);
	} else {
		/* TODO: Multicast. */
		LIST_FOREACH(&running_cpus, iter) {
			cpu = list_entry(iter, cpu_t, header);
			if(cpu == curr_cpu || !cpu->aspace || ctx != cpu->aspace->mmu) {
				continue;
			}

			/* CPU is using this address space. */
			if(smp_call_single(cpu->id, tlb_invalidate_call_func, ctx,
			                   0) != STATUS_SUCCESS) {
				fatal("Could not perform remote TLB invalidation");
			}
		}
	}

	ctx->invalidate_count = 0;
}
#endif

/** Unlock an MMU context.
 * @param ctx		Context to unlock. */
void mmu_context_unlock(mmu_context_t *ctx) {
#if CONFIG_SMP
	/* If the lock is being released (recursion count currently 1), flush
	 * queued TLB changes. */
	if(mutex_recursion(&ctx->lock) == 1) {
		mmu_context_flush(ctx);
	}
#endif
	mutex_unlock(&ctx->lock);
	thread_unwire(curr_thread);
}

/** Create a mapping.
 * @param ctx		Context to map into.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to.
 * @param write		Whether the mapping should be writable.
 * @param execute	Whether the mapping should be executable.
 * @param mmflag	Memory allocation behaviour flags.
 * @return		Status code describing the result of the operation. */
status_t mmu_context_map(mmu_context_t *ctx, ptr_t virt, phys_ptr_t phys, bool write, bool execute, int mmflag) {
	uint64_t *ptbl, flags;
	unsigned type, pte;

	CHECK_OPERATION(ctx, virt, phys);

	/* Find the page table for the entry. */
	ptbl = mmu_context_get_ptbl(ctx, virt, true, mmflag);
	if(!ptbl) {
		return STATUS_NO_MEMORY;
	}

	/* Check that the mapping doesn't already exist. */
	pte = (virt % 0x200000) / PAGE_SIZE;
	if(unlikely(ptbl[pte] & X86_PTE_PRESENT)) {
		fatal("Mapping %p which is already mapped", virt);
	}

	/* Determine mapping flags. Kernel mappings have the global flag set. */
	flags = X86_PTE_PRESENT;
	if(write) {
		flags |= X86_PTE_WRITE;
	}
	if(!execute && cpu_features.xd) {
		flags |= X86_PTE_NOEXEC;
	}
	if(IS_KERNEL_CTX(ctx)) {
		flags |= X86_PTE_GLOBAL;
	} else {
		flags |= X86_PTE_USER;
	}

	/* Get the memory type of the address and set flags accordingly. Only
	 * use flags that require the PAT if the PAT is supported. */
	type = phys_memory_type(phys);
	if(!memory_type_flags[type].pat || cpu_features.pat) {
		flags |= memory_type_flags[type].flags;
	}

	/* Set the PTE. */
	ptbl[pte] = phys | flags;
	memory_barrier();
	return STATUS_SUCCESS;
}

/** Modify protection flags of a mapping.
 * @param ctx		Context to modify in.
 * @param virt		Virtual address to modify.
 * @param write		Whether to make the mapping writable.
 * @param execute	Whether to make the mapping executable. */
void mmu_context_protect(mmu_context_t *ctx, ptr_t virt, bool write, bool execute) {
	uint64_t *ptbl;
	unsigned pte;

	CHECK_OPERATION(ctx, virt, 0);

	/* Find the page table for the entry. */
	ptbl = mmu_context_get_ptbl(ctx, virt, false, 0);
	if(!ptbl) {
		return;
	}

	/* If the mapping doesn't exist we don't need to do anything. */
	pte = (virt % 0x200000) / PAGE_SIZE;
	if(!(ptbl[pte] & X86_PTE_PRESENT)) {
		return;
	}

	/* Update the entry. */
	if(write) {
		ptbl[pte] |= X86_PTE_WRITE;
	} else {
		ptbl[pte] &= ~X86_PTE_WRITE;
	}
	if(execute) {
		ptbl[pte] &= ~X86_PTE_NOEXEC;
	} else if(cpu_features.xd) {
		ptbl[pte] |= X86_PTE_NOEXEC;
	}
	memory_barrier();

	/* Clear TLB entries if necessary (see note in mmu_context_unmap()). */
	if(ptbl[pte] & X86_PTE_ACCESSED) {
		mmu_context_invalidate(ctx, virt, true);
	}
}

/** Remove a mapping.
 * @param ctx		Context to unmap from.
 * @param virt		Virtual address to unmap.
 * @param shared	Whether the mapping was shared across multiple CPUs.
 *			Used as an optimisation to not perform remote TLB
 *			invalidations if not necessary.
 * @param physp		Where to store physical address of mapping.
 * @return		Whether the address was mapped prior to the call. */
bool mmu_context_unmap(mmu_context_t *ctx, ptr_t virt, bool shared, phys_ptr_t *physp) {
	phys_ptr_t paddr;
	uint64_t *ptbl;
	bool accessed;
	unsigned pte;
	page_t *page;

	CHECK_OPERATION(ctx, virt, 0);

	/* Find the page table for the entry. */
	ptbl = mmu_context_get_ptbl(ctx, virt, false, 0);
	if(!ptbl) {
		return false;
	}

	/* If the mapping doesn't exist we don't need to do anything. */
	pte = (virt % 0x200000) / PAGE_SIZE;
	if(!(ptbl[pte] & X86_PTE_PRESENT)) {
		return false;
	}

	/* Save the physical address to return. */
	paddr = ptbl[pte] & PHYS_PAGE_MASK;

	/* If the entry is dirty, set the modified flag on the page. */
	if(ptbl[pte] & X86_PTE_DIRTY) {
		page = page_lookup(paddr);
		if(page) {
			page->modified = true;
		}
	}

	/* If the entry has been accessed, need to flush TLB entries. A
	 * processor will not cache a translation without setting the accessed
	 * flag first (Intel Vol. 3A Section 4.10.2.3 "Details of TLB Use"). */
	accessed = (ptbl[pte] & X86_PTE_ACCESSED);

	/* Clear the entry and invalidate the TLB entry. */
	ptbl[pte] = 0;
	memory_barrier();
	if(accessed) {
		mmu_context_invalidate(ctx, virt, shared);
	}

	if(physp) {
		*physp = paddr;
	}
	return true;
}

/** Query details about a mapping.
 * @param ctx		Context to query in.
 * @param virt		Virtual address to look up.
 * @param physp		Where to store physical address mapped to.
 * @param writep	Where to store whether the mapping is writeable.
 * @param executep	Where to store whether the mapping is executable.
 * @return		Whether the mapping exists. */
bool mmu_context_query(mmu_context_t *ctx, ptr_t virt, phys_ptr_t *physp, bool *writep, bool *executep) {
	uint64_t *pdir, *ptbl;
	unsigned pde, pte;

	/* We allow checks on any address here, so that you can query a kernel
	 * address even when you are on a user address space. */
	assert(mutex_held(&ctx->lock));
	assert(!(virt % PAGE_SIZE));

	/* Find the page directory for the entry. */
	pdir = mmu_context_get_pdir(ctx, virt, false, 0);
	if(pdir) {
		/* Get the page table number. A page table covers 2MB. */
		pde = (virt % 0x40000000) / 0x200000;
		if(pdir[pde] & X86_PTE_PRESENT) {
			/* Handle large pages: parts of the kernel address
			 * space may be mapped with large pages, so we must
			 * be able to handle queries on these parts. */
			if(pdir[pde] & X86_PTE_LARGE) {
				if(physp) {
					*physp = (pdir[pde] & 0x000000FFFFF00000L) + (virt % 0x200000);
				}
				if(writep) {
					*writep = !!(pdir[pde] & X86_PTE_WRITE);
				}
				if(executep) {
					*executep = (pdir[pde] & X86_PTE_NOEXEC) == 0;
				}
				return true;
			}

			/* Not a large page, map page table. */
			ptbl = map_structure(pdir[pde] & PHYS_PAGE_MASK);
			pte = (virt % 0x200000) / PAGE_SIZE;
			if(ptbl[pte] & X86_PTE_PRESENT) {
				if(physp) {
					*physp = ptbl[pte] & PHYS_PAGE_MASK;
				}
				if(writep) {
					*writep = !!(pdir[pde] & X86_PTE_WRITE);
				}
				if(executep) {
					*executep = (pdir[pde] & X86_PTE_NOEXEC) == 0;
				}
				return true;
			}
		}
	}

	return false;
}

/** Switch to another MMU context.
 * @param ctx		Context to switch to. */
void mmu_context_switch(mmu_context_t *ctx) {
	x86_write_cr3(ctx->pml4);
}

/** Create and initialise an MMU context.
 * @param mmflag	Allocation behaviour flags.
 * @return		Pointer to new context, NULL on allocation failure. */
mmu_context_t *mmu_context_create(int mmflag) {
	uint64_t *kpml4, *pml4;
	mmu_context_t *ctx;

	ctx = kmalloc(sizeof(*ctx), mmflag);
	if(!ctx) {
		return NULL;
	}

	mutex_init(&ctx->lock, "mmu_context_lock", MUTEX_RECURSIVE);
	ctx->invalidate_count = 0;
	ctx->pml4 = alloc_structure(mmflag);
	if(!ctx->pml4) {
		kfree(ctx);
		return NULL;
	}

	/* Get the kernel mappings into the new PML4. */
	kpml4 = map_structure(kernel_mmu_context.pml4);
	pml4 = map_structure(ctx->pml4);
	pml4[511] = kpml4[511] & ~X86_PTE_ACCESSED;
	return ctx;
}

/**
 * Destroy an MMU context.
 *
 * Destroys an MMU context. Will not free any pages that have been mapped into
 * the address space - this should be done by the caller.
 *
 * @param ctx		Context to destroy.
 */
void mmu_context_destroy(mmu_context_t *ctx) {
	uint64_t *pml4, *pdp, *pdir;
	unsigned i, j, k;

	assert(!IS_KERNEL_CTX(ctx));

	/* Free all structures in the bottom half of the PML4 (user memory). */
	pml4 = map_structure(ctx->pml4);
	for(i = 0; i < 256; i++) {
		if(!(pml4[i] & X86_PTE_PRESENT)) {
			continue;
		}

		pdp = map_structure(pml4[i] & PHYS_PAGE_MASK);
		for(j = 0; j < 512; j++) {
			if(!(pdp[j] & X86_PTE_PRESENT)) {
				continue;
			}

			pdir = map_structure(pdp[j] & PHYS_PAGE_MASK);
			for(k = 0; k < 512; k++) {
				if(!(pdir[k] & X86_PTE_PRESENT)) {
					continue;
				}

				assert(!(pdir[k] & X86_PTE_LARGE));

				phys_free(pdir[k] & PHYS_PAGE_MASK, PAGE_SIZE);
			}

			phys_free(pdp[j] & PHYS_PAGE_MASK, PAGE_SIZE);
		}

		phys_free(pml4[i] & PHYS_PAGE_MASK, PAGE_SIZE);
	}

	phys_free(ctx->pml4, PAGE_SIZE);
	kfree(ctx);
}

/** Create a kernel mapping.
 * @param core		KBoot core tag pointer.
 * @param start		Start of range to map.
 * @param end		End of range to map.
 * @param write		Whether to mark as writable.
 * @param execute	Whether to mark as executable. */
static __init_text void create_kernel_mapping(kboot_tag_core_t *core, ptr_t start, ptr_t end,
                                              bool write, bool execute) {
	phys_ptr_t phys = (start - KERNEL_VIRT_BASE) + core->kernel_phys;
	size_t i;

	assert(start >= KERNEL_VIRT_BASE);
	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));

	for(i = 0; i < (end - start); i += PAGE_SIZE) {
		mmu_context_map(&kernel_mmu_context, start + i, phys + i, write, execute, MM_FATAL);
	}

	kprintf(LOG_DEBUG, "mmu: created kernel mapping [%p,%p) to [0x%" PRIxPHYS ",0x%" PRIxPHYS ") (write: %d, exec: %d)\n",
	        start, end, phys, phys + (end - start), write, execute);
}

/** Create the kernel MMU context. */
__init_text void arch_mmu_init(void) {
	phys_ptr_t i, j, highest_phys = 0;
	kboot_tag_core_t *core;
	uint64_t *pdir;

#if CONFIG_SMP
	/* Reserve a low memory page for the AP bootstrap code. */
	phys_alloc(PAGE_SIZE, 0, 0, 0, 0x100000, MM_FATAL, &ap_bootstrap_page);
#endif

	/* Initialise the kernel MMU context structure. */
	mutex_init(&kernel_mmu_context.lock, "mmu_context_lock", MUTEX_RECURSIVE);
	kernel_mmu_context.invalidate_count = 0;
	kernel_mmu_context.pml4 = alloc_structure(MM_FATAL);

	/* We require the core tag to get the kernel physical address. */
	core = kboot_tag_iterate(KBOOT_TAG_CORE, NULL);
	assert(core);

	mmu_context_lock(&kernel_mmu_context);

	/* Map the kernel in. The following mappings are made:
	 *  .text      - R/X
	 *  .init      - R/W/X
	 *  .rodata    - R
	 *  .data/.bss - R/W */
	create_kernel_mapping(core, ROUND_DOWN((ptr_t)__text_start, PAGE_SIZE), (ptr_t)__text_end, false, true);
	create_kernel_mapping(core, (ptr_t)__init_start, (ptr_t)__init_end, true, true);
	create_kernel_mapping(core, (ptr_t)__rodata_start, (ptr_t)__rodata_end, false, false);
	create_kernel_mapping(core, (ptr_t)__data_start, (ptr_t)__bss_end, true, false);

	kboot_tag_release(core);

	/* Search for the highest physical address we have in the memory map. */
	KBOOT_ITERATE(KBOOT_TAG_MEMORY, kboot_tag_memory_t, range) {
		if(range->end > highest_phys) {
			highest_phys = range->end;
		}
	}

	/* We always map at least 8GB, and align to a 1GB boundary. */
	highest_phys = ROUND_UP(MAX(0x200000000UL, highest_phys), 0x40000000UL);
	kprintf(LOG_DEBUG, "mmu: mapping physical memory up to 0x%" PRIxPHYS "\n",
	        highest_phys);

	/* Create the physical map area. */
	for(i = 0; i < highest_phys; i += 0x40000000) {
		pdir = mmu_context_get_pdir(&kernel_mmu_context, i + KERNEL_PMAP_BASE, true, MM_FATAL);
		for(j = 0; j < 0x40000000; j += LARGE_PAGE_SIZE) {
			pdir[j / LARGE_PAGE_SIZE] = (i + j) | X86_PTE_PRESENT | X86_PTE_WRITE |
			                            X86_PTE_GLOBAL | X86_PTE_LARGE;
		}
	}

	mmu_context_unlock(&kernel_mmu_context);
}

/** Get a PAT entry. */
#define PAT(e, t)	((uint64_t)t << ((e) * 8))

/** Initialise the MMU for this CPU. */
__init_text void arch_mmu_init_percpu(void) {
	uint64_t pat;

	/* Configure the PAT. We do not use the PAT bit in the page table, as
	 * conflicts with the large page bit, so we make PAT3 be WC. */
	if(cpu_features.pat) {
		pat = PAT(0, 0x06) | PAT(1, 0x04) | PAT(2, 0x07) | PAT(3, 0x01) |
		      PAT(4, 0x06) | PAT(5, 0x04) | PAT(6, 0x07) | PAT(7, 0x00);
		x86_write_msr(X86_MSR_CR_PAT, pat);
	}
}
