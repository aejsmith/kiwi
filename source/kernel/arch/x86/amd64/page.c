/*
 * Copyright (C) 2009-2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		AMD64 paging functions.
 */

#include <arch/barrier.h>
#include <arch/memory.h>

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>
#include <mm/vm.h>

#include <proc/thread.h>

#include <assert.h>
#include <console.h>
#include <kargs.h>
#include <status.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Check if a page map is the kernel page map. */
#define IS_KERNEL_MAP(map)	(map == &kernel_page_map)

/** Return flags to map a PDP/page directory/page table with. */
#define TABLE_MAPPING_FLAGS(map)	\
	(IS_KERNEL_MAP(map) ? (PG_PRESENT | PG_WRITE) : (PG_PRESENT | PG_WRITE | PG_USER))

/** Determine if a page map is the current page map. */
#define IS_CURRENT_MAP(map)	(IS_KERNEL_MAP(map) || map == &curr_aspace->pmap)

extern char __text_start[], __text_end[];
extern char __init_start[], __init_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __bss_end[];

/** Kernel page map. */
page_map_t kernel_page_map;

/** Whether the kernel page map has been initialised. */
static bool paging_inited = false;

/** Invalidate a TLB entry.
 * @param addr		Address to invalidate. */
static inline void invlpg(ptr_t addr) {
	__asm__ volatile("invlpg (%0)" :: "r"(addr));
}

/** Allocate a paging structure.
 * @param mmflag	Allocation flags. PM_ZERO will be added.
 * @return		Address of structure on success, 0 on failure. */
static phys_ptr_t page_structure_alloc(int mmflag) {
	if(likely(paging_inited)) {
		return page_alloc(1, mmflag | PM_ZERO);
	} else {
		/* During initialisation we only have 1GB of physical mapping. */
		return page_xalloc(1, 0, 0, 0x40000000, mmflag | PM_ZERO);
	}
}

/** Get the virtual address of a page structure.
 * @param addr		Address of structure.
 * @return		Pointer to mapping. */
static uint64_t *page_structure_map(phys_ptr_t addr) {
	/* Our page_phys_map() implementation never fails. */
	return page_phys_map(addr, PAGE_SIZE, MM_FATAL);
}

/** Add an address to the invalidation list.
 * @param map		Map to add to.
 * @param virt		Address to add. */
static void page_map_add_to_invalidate(page_map_t *map, ptr_t virt) {
	if(map->invalidate_count < INVALIDATE_ARRAY_SIZE) {
		map->pages_to_invalidate[map->invalidate_count] = virt;
	}
	map->invalidate_count++;
}

/** Get the page directory containing an address.
 * @param map		Page map to get from.
 * @param virt		Virtual address to get page directory for.
 * @param alloc		Whether to allocate new tables if not found.
 * @param mmflag	Allocation flags.
 * @return		Virtual address of mapping, NULL on failure. If alloc
 *			is true, failure can only occur due to allocation
 *			failure. Otherwise, failure can only occur if the
 *			directory is not present. */
static uint64_t *page_map_get_pdir(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	uint64_t *pml4, *pdp;
	phys_ptr_t page;
	int pml4e, pdpe;

	/* Get the virtual address of the PML4. */
	pml4 = page_structure_map(map->cr3);

	/* Get the page directory pointer number. A PDP covers 512GB. */
	pml4e = (virt & 0x0000FFFFFFFFF000) / 0x8000000000;
	if(!(pml4[pml4e] & PG_PRESENT)) {
		/* Allocate a new PDP if required. Allocating a page can cause
		 * page mappings to be modified (if a vmem boundary tag refill
		 * occurs), handle this possibility. */
		if(alloc) {
			page = page_structure_alloc(mmflag);
			if(pml4[pml4e] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(unlikely(!page)) {
					return NULL;
				}

				/* Map it into the PML4. */
				pml4[pml4e] = page | TABLE_MAPPING_FLAGS(map);
			}
		} else {
			return NULL;
		}
	}

	/* Get the PDP from the PML4. */
	pdp = page_structure_map(pml4[pml4e] & PHYS_PAGE_MASK);

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!(pdp[pdpe] & PG_PRESENT)) {
		/* Allocate a new page directory if required. See note above. */
		if(alloc) {
			page = page_structure_alloc(mmflag);
			if(pdp[pdpe] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(unlikely(!page)) {
					return NULL;
				}

				/* Map it into the PDP. */
				pdp[pdpe] = page | TABLE_MAPPING_FLAGS(map);
			}
		} else {
			return NULL;
		}
	}

	/* Return the page directory address. */
	return page_structure_map(pdp[pdpe] & PHYS_PAGE_MASK);
}

/** Get the page table containing an address.
 * @param map		Page map to get from.
 * @param virt		Virtual address to get page table for.
 * @param alloc		Whether to allocate new tables if not found.
 * @param mmflag	Allocation flags.
 * @return		Virtual address of mapping, NULL on failure. */
static uint64_t *page_map_get_ptbl(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	phys_ptr_t page;
	uint64_t *pdir;
	int pde;

	/* Get hold of the page directory. */
	if(!(pdir = page_map_get_pdir(map, virt, alloc, mmflag))) {
		return NULL;
	}

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / LARGE_PAGE_SIZE;
	if(!(pdir[pde] & PG_PRESENT)) {
		/* Allocate a new page table if required. See note above. */
		if(alloc) {
			page = page_structure_alloc(mmflag);
			if(pdir[pde] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(unlikely(!page)) {
					return NULL;
				}

				/* Map it into the page directory. */
				pdir[pde] = page | TABLE_MAPPING_FLAGS(map);
			}
		} else {
			return NULL;
		}
	}

	/* If this function is being used it should not be a large page. */
	assert(!(pdir[pde] & PG_LARGE));
	return page_structure_map(pdir[pde] & PHYS_PAGE_MASK);
}

/** Lock a page map.
 *
 * Locks the specified page map. This must be done before performing any
 * operations on it, and it must be unlocked with page_map_unlock() after
 * operations have bene performed. Locks can be nested (implemented using a
 * recursive mutex).
 *
 * @param map		Map to lock.
 */
void page_map_lock(page_map_t *map) {
	thread_wire(curr_thread);
	mutex_lock(&map->lock);
}

/** TLB invalidation IPI handler.
 * @param d1		Address of page map structure.
 * @return		Always returns STATUS_SUCCESS. */
static status_t tlb_invalidate_ipi(void *msg, unative_t d1, unative_t d2, unative_t d3, unative_t d4) {
	page_map_t *map = (page_map_t *)((ptr_t)d1);
	size_t i;

	/* Don't need to do anything if we aren't using the page map - the
	 * CPU may have switched address space between sending the IPI and
	 * receiving it. */
	if(IS_CURRENT_MAP(map)) {
		/* If the number of pages to invalidate is larger than the size
		 * of the address array, perform a complete TLB flush. */
		if(map->invalidate_count > INVALIDATE_ARRAY_SIZE) {
			dprintf("page: performing full TLB flush for map %p on %u\n",
			        map, curr_cpu->id);

			/* For the kernel page map, we must disable PGE and
			 * reenable it to perform a complete TLB flush. */
			if(IS_KERNEL_MAP(map)) {
				x86_write_cr4(x86_read_cr4() & ~X86_CR4_PGE);
				x86_write_cr4(x86_read_cr4() | X86_CR4_PGE);
			} else {
				x86_write_cr3(x86_read_cr3());
			}
		} else {
			for(i = 0; i < map->invalidate_count; i++) {
				dprintf("page: invalidating address %p for map %p on %u\n",
				        map->pages_to_invalidate[i], map, curr_cpu->id);
				invlpg(map->pages_to_invalidate[i]);
			}
		}
	}

	return STATUS_SUCCESS;
}

/** Send invalidation IPIs.
 * @param map		Map to send for. */
static void page_map_flush(page_map_t *map) {
	cpu_t *cpu;

	/* Check if anything needs to be done. */
	if(cpu_count < 2 || !map->invalidate_count) {
		map->invalidate_count = 0;
		return;
	}

	/* If this is the kernel page map, perform changes on all other CPUs,
	 * else perform it on each CPU using the map. */
	if(IS_KERNEL_MAP(map)) {
		ipi_broadcast(tlb_invalidate_ipi, (unative_t)map, 0, 0, 0, IPI_SEND_SYNC);
	} else {
		/* TODO: Multicast. */
		LIST_FOREACH(&cpus_running, iter) {
			cpu = list_entry(iter, cpu_t, header);
			if(cpu == curr_cpu || map != &cpu->aspace->pmap) {
				continue;
			}

			/* CPU is using this address space. */
			if(ipi_send(cpu->id, tlb_invalidate_ipi, (unative_t)map,
			            0, 0, 0, IPI_SEND_SYNC) != STATUS_SUCCESS) {
				fatal("Could not send TLB invalidation IPI");
			}
		}
	}

	map->invalidate_count = 0;
}

/** Unlock a page map.
 * @param map		Map to unlock. */
void page_map_unlock(page_map_t *map) {
	/* If the lock is being released (recursion count currently 1), and
	 * flush queued TLB changes. */
	if(mutex_recursion(&map->lock) == 1) {
		page_map_flush(map);
	}

	mutex_unlock(&map->lock);
	thread_unwire(curr_thread);
}

/** Map a page into a page map.
 * @param map		Page map to map into.
 * @param virt		Virtual address to map.
 * @param phys		Page to map to.
 * @param write		Whether to make the mapping writable.
 * @param exec		Whether to make the mapping executable.
 * @param mmflag	Allocation flags.
 * @return		Status code describing result of operation. */
status_t page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, bool write, bool exec, int mmflag) {
	uint64_t *ptbl, flags;
	memory_type_t type;
	int pte;

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, true, mmflag);
	if(!ptbl) {
		return STATUS_NO_MEMORY;
	}

	/* Check that the mapping doesn't already exist. */
	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	if(ptbl[pte] & PG_PRESENT) {
		fatal("Mapping %p which is already mapped", virt);
	}

	/* Determine mapping flags. Kernel mappings have the global flag set. */
	flags = PG_PRESENT;
	if(write) {
		flags |= PG_WRITE;
	}
#if CONFIG_X86_NX
	if(!exec && cpu_features.xd) {
		flags |= PG_NOEXEC;
	}
#endif
	if(IS_KERNEL_MAP(map)) {
		flags |= PG_GLOBAL;
	} else {
		flags |= PG_USER;
	}

	/* Get the memory type of the address and set flags accordingly. */
	type = MEMORY_TYPE_WB;
	page_get_memory_type(phys, &type);
	switch(type) {
	case MEMORY_TYPE_UC:
		flags |= PG_PCD;
		break;
	case MEMORY_TYPE_WC:
		/* This is only supported if the PAT is supported, we configure
		 * it to use WC when both PCD and PWT are set. */
		if(cpu_features.pat) {
			flags |= (PG_PCD | PG_PWT);
		}
		break;
	case MEMORY_TYPE_WT:
		flags |= PG_PWT;
		break;
	case MEMORY_TYPE_WB:
		/* No extra flags means WB. */
		break;
	}

	/* Set the PTE. */
	ptbl[pte] = phys | flags;
	memory_barrier();
	return STATUS_SUCCESS;
}

/** Modify protection flags on a mapping.
 * @param map		Page map to modify in.
 * @param virt		Virtual address to modify.
 * @param write		Whether to make the mapping writable.
 * @param exec		Whether to make the mapping executable. */
void page_map_protect(page_map_t *map, ptr_t virt, bool write, bool exec) {
	uint64_t *ptbl;
	int pte;

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));

	/* Find the page table for the entry. */
	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP);
	if(!ptbl) {
		return;
	} else if(!ptbl[pte] & PG_PRESENT) {
		return;
	}

	/* Update the entry. */
	if(write) {
		ptbl[pte] |= PG_WRITE;
	} else {
		ptbl[pte] &= ~PG_WRITE;
	}
#if CONFIG_X86_NX
	if(exec) {
		ptbl[pte] &= ~PG_NOEXEC;
	} else if(cpu_features.xd) {
		ptbl[pte] |= PG_NOEXEC;
	}
#endif
	memory_barrier();

	/* Clear TLB entries. */
	if(IS_CURRENT_MAP(map)) {
		invlpg(virt);
	}
	page_map_add_to_invalidate(map, virt);
}

/** Unmap a page.
 * @param map		Page map to unmap from.
 * @param virt		Virtual address to unmap.
 * @param shared	Whether the mapping was shared across multiple CPUs.
 *			Used as an optimisation to not perform remote TLB
 *			invalidations if not necessary.
 * @param physp		Where to store physical address that was mapped.
 * @return		Whether the address was mapped. */
bool page_map_remove(page_map_t *map, ptr_t virt, bool shared, phys_ptr_t *physp) {
	phys_ptr_t paddr;
	vm_page_t *page;
	uint64_t *ptbl;
	int pte;

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));

	/* Find the page table for the entry. */
	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP);
	if(!ptbl) {
		return false;
	} else if(!ptbl[pte] & PG_PRESENT) {
		return false;
	}

	paddr = ptbl[pte] & PHYS_PAGE_MASK;

	/* If the entry is dirty, set the modified flag on the page. */
	if(ptbl[pte] & PG_DIRTY && (page = vm_page_lookup(paddr))) {
		page->modified = true;
	}

	/* If the entry has been accessed, need to flush TLB entries. */
	if(ptbl[pte] & PG_ACCESSED) {
		if(IS_CURRENT_MAP(map)) {
			invlpg(virt);
		}
		if(shared) {
			page_map_add_to_invalidate(map, virt);
		}
	}

	/* Clear the entry. */
	ptbl[pte] = 0;
	memory_barrier();

	if(physp) {
		*physp = paddr;
	}
	return true;
}

/** Find the page a virtual address is mapped to.
 * @param map		Page map to find in.
 * @param virt		Virtual address to find.
 * @param physp		Where to store physical address.
 * @return		Whether the virtual address was mapped. */
bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	uint64_t *pdir, *ptbl;
	int pde, pte;

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));
	assert(physp);

	/* Find the page directory for the entry. */
	pdir = page_map_get_pdir(map, virt, false, MM_SLEEP);
	if(pdir) {
		/* Get the page table number. A page table covers 2MB. */
		pde = (virt % 0x40000000) / LARGE_PAGE_SIZE;
		if(pdir[pde] & PG_PRESENT) {
			/* Handle large pages. */
			if(pdir[pde] & PG_LARGE) {
				*physp = (pdir[pde] & PHYS_PAGE_MASK) + (virt % 0x200000);
				return true;
			}

			ptbl = page_structure_map(pdir[pde] & PHYS_PAGE_MASK);
			if(ptbl) {
				pte = (virt % 0x200000) / PAGE_SIZE;
				if(ptbl[pte] & PG_PRESENT) {
					*physp = ptbl[pte] & PHYS_PAGE_MASK;
					return true;
				}
			}
		}
	}

	return false;
}

/** Switch to a page map.
 * @param map		Page map to switch to. */
void page_map_switch(page_map_t *map) {
	x86_write_cr3(map->cr3);
}

/** Initialise a page map.
 * @param map		Page map to initialise.
 * @param mmflag	Allocation flags.
 * @return		Status code describing result of operation. Failure
 *			can only occur if MM_SLEEP is not specified. */
status_t page_map_init(page_map_t *map, int mmflag) {
	uint64_t *kpml4, *pml4;

	mutex_init(&map->lock, "page_map_lock", MUTEX_RECURSIVE);
	map->invalidate_count = 0;
	map->cr3 = page_structure_alloc(mmflag);
	if(!map->cr3) {
		return STATUS_NO_MEMORY;
	}

	if(!IS_KERNEL_MAP(map)) {
		/* Get the kernel mappings into the new PML4. */
		kpml4 = page_structure_map(kernel_page_map.cr3);
		pml4 = page_structure_map(map->cr3);
		pml4[511] = kpml4[511] & ~PG_ACCESSED;
	}

	return STATUS_SUCCESS;
}

/** Destroy a page map.
 *
 * Destroys a page map. Will not free any pages that have been mapped into the
 * page map - this should be done by the caller.
 *
 * @param map		Page map to destroy.
 */
void page_map_destroy(page_map_t *map) {
	uint64_t *pml4, *pdp, *pdir;
	int i, j, k;

	assert(!IS_KERNEL_MAP(map));

	/* Free all structures in the bottom half of the PML4 (user memory). */
	pml4 = page_structure_map(map->cr3);
	for(i = 0; i < 256; i++) {
		if(!(pml4[i] & PG_PRESENT)) {
			continue;
		}

		pdp = page_structure_map(pml4[i] & PHYS_PAGE_MASK);
		for(j = 0; j < 512; j++) {
			if(!(pdp[j] & PG_PRESENT)) {
				continue;
			}

			pdir = page_structure_map(pdp[j] & PHYS_PAGE_MASK);
			for(k = 0; k < 512; k++) {
				if(!(pdir[k] & PG_PRESENT)) {
					continue;
				}

				if(!(pdir[k] & PG_LARGE)) {
					page_free(pdir[k] & PHYS_PAGE_MASK, 1);
				}
			}

			page_free(pdp[j] & PHYS_PAGE_MASK, 1);
		}

		page_free(pml4[i] & PHYS_PAGE_MASK, 1);
	}

	page_free(map->cr3, 1);
}

/** Map physical memory into the kernel address space.
 * @note		The range does not need to be page-aligned.
 * @param addr		Physical address to map.
 * @param size		Size of range to map.
 * @param mmflag	Allocation behaviour flags.
 * @return		Address of mapping or NULL on failure. */
void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag) {
	if(!size) {
		return NULL;
	}

	if(likely(paging_inited)) {
		return (void *)(KERNEL_PMAP_BASE + addr);
	} else {
		/* During boot there is a 1GB identity mapping. */
		assert(addr < 0x40000000);
		assert((addr + size) <= 0x40000000);
		return (void *)addr;
	}
}

/** Unmap physical memory from the kernel address space.
 * @param addr		Address to unmap.
 * @param size		Size of mapping.
 * @param shared	Whether the mapping was accessed by any CPUs other than
 *			the CPU that mapped it. This is used as an optimisation
 *			to not perform remote TLB invalidations when not
 *			required. */
void page_phys_unmap(void *addr, size_t size, bool shared) {
	/* Nothing needs to be done. */
}

/** Map part of the kernel into the kernel page map.
 * @param args		Kernel arguments pointer.
 * @param start		Start of range to map.
 * @param end		End of range to map.
 * @param write		Whether to mark as writable.
 * @param exec		Whether to mark as executable. */
static void __init_text page_map_kernel_range(kernel_args_t *args, ptr_t start, ptr_t end,
                                              bool write, bool exec) {
	phys_ptr_t phys = (start - KERNEL_VIRT_BASE) + args->kernel_phys;
	size_t i;

	assert(start >= KERNEL_VIRT_BASE);
	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));

	for(i = 0; i < (end - start); i += PAGE_SIZE) {
		page_map_insert(&kernel_page_map, start + i, phys + i, write, exec, MM_FATAL);
	}

	dprintf("page: created kernel mapping [%p,%p) to [0x%" PRIpp ",0x%" PRIpp ") (%d %d)\n",
	        start, end, phys, phys + (end - start), write, exec);
}

/** Perform AMD64 paging initialisation.
 * @param args		Kernel arguments pointer. */
void __init_text page_arch_init(kernel_args_t *args) {
	uint64_t *pml4, *bpml4, *pdir;
	phys_ptr_t i, j;

	/* Initialise the kernel page map structure. */
	page_map_init(&kernel_page_map, MM_FATAL);
	page_map_lock(&kernel_page_map);

	/* Map the kernel in. The following mappings are made:
	 *  .text      - R/X
	 *  .init      - R/W/X
	 *  .rodata    - R
	 *  .data/.bss - R/W */
	page_map_kernel_range(args, ROUND_DOWN((ptr_t)__text_start, PAGE_SIZE), (ptr_t)__text_end, false, true);
	page_map_kernel_range(args, (ptr_t)__init_start, (ptr_t)__init_end, true, true);
	page_map_kernel_range(args, (ptr_t)__rodata_start, (ptr_t)__rodata_end, false, false);
	page_map_kernel_range(args, (ptr_t)__data_start, (ptr_t)__bss_end, true, false);

	/* Create 8GB of physical mapping for now. FIXME. */
	for(i = 0; i < 0x200000000; i += 0x40000000) {
		pdir = page_map_get_pdir(&kernel_page_map, i + KERNEL_PMAP_BASE, true, MM_FATAL);
		for(j = 0; j < 0x40000000; j += LARGE_PAGE_SIZE) {
			pdir[j / LARGE_PAGE_SIZE] = (i + j) | PG_PRESENT | PG_WRITE | PG_GLOBAL | PG_LARGE;
		}
	}

	/* The temporary identity mapping is still required as all the CPUs'
	 * stack pointers are in it, and the kernel arguments pointer points to
	 * it. Use the structures from the bootloader rather than just using
	 * the new kernel PDP because the kernel PDP has the global flag set
	 * on all pages, which makes invalidating the TLB entries difficult
	 * when removing the mapping. */
	pml4 = page_structure_map(kernel_page_map.cr3);
	bpml4 = page_structure_map(x86_read_cr3() & PHYS_PAGE_MASK);
	pml4[0] = bpml4[0];

	page_map_unlock(&kernel_page_map);
	dprintf("page: initialised kernel page map (pml4: 0x%" PRIpp ")\n",
	        kernel_page_map.cr3);

	/* Switch to the kernel page map. */
	page_map_switch(&kernel_page_map);

	/* The physical map area can now be used. */
	paging_inited = true;
}

/** TLB flush IPI handler.
 * @return		Always returns STATUS_SUCCESS. */
static status_t tlb_flush_ipi(void *msg, unative_t d1, unative_t d2, unative_t d3, unative_t d4) {
	x86_write_cr3(x86_read_cr3());
	return STATUS_SUCCESS;
}

/** Perform late AMD64 paging initialisation. */
void page_arch_late_init(void) {
	uint64_t *pml4;

	/* All of the CPUs have been booted and have new stacks, and the kernel
	 * arguments are no longer required. Remove the temporary identity
	 * mapping and flush the TLB on all CPUs. */
	pml4 = page_structure_map(kernel_page_map.cr3);
	pml4[0] = 0;
	x86_write_cr3(x86_read_cr3());
	ipi_broadcast(tlb_flush_ipi, 0, 0, 0, 0, IPI_SEND_SYNC);
}

/** Get a PAT entry. */
#define PAT(e, t)	((uint64_t)t << ((e) * 8))

/** Initialise the PAT. */
void __init_text pat_init(void) {
	uint64_t pat;

	/* Configure the PAT. We do not use the PAT bit in the page table, as
	 * conflicts with the large page bit, so we make PAT3 be WC. */
	if(cpu_features.pat) {
		pat = PAT(0, 0x06) | PAT(1, 0x04) | PAT(2, 0x07) | PAT(3, 0x01) |
		      PAT(4, 0x06) | PAT(5, 0x04) | PAT(6, 0x07) | PAT(7, 0x00);
		x86_write_msr(X86_MSR_CR_PAT, pat);
	}
}
