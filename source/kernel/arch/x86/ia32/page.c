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
 * @brief		IA32 paging functions.
 */

#include <arch/x86/cpu.h>
#include <arch/x86/page.h>
#include <arch/barrier.h>
#include <arch/memory.h>

#include <cpu/cpu.h>
#include <cpu/ipi.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kheap.h>
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

/** Macro to get mapping for a kernel page table.
 * @param addr		Virtual address to get page table for. */
#define KERNEL_PTBL_ADDR(addr)	(KERNEL_PTBL_BASE + (((addr % 0x40000000) / 0x200000) * 0x1000))

/** Macro expanding to the address of the kernel page directory mapping. */
#define KERNEL_PDIR_ADDR	KERNEL_PTBL_ADDR(KERNEL_PTBL_BASE)

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
 * @note		The structure will not be zeroed unless PM_ZERO is
 *			specified, and this should only be done in cases where
 *			it is safe.
 * @param mmflag	Allocation flags.
 * @return		Address of structure on success, 0 on failure. */
static phys_ptr_t page_structure_alloc(int mmflag) {
	phys_ptr_t page;

	/* Prefer allocating structures below 1GB because pages in the first
	 * 1GB of memory are always mapped in. During initialisation, must
	 * always allocate below 1GB because the heap is not set up. */
	page = page_xalloc(1, 0, 0, 0x40000000, (paging_inited) ? (mmflag & ~MM_FATAL) : mmflag);
	if(!page) {
		page = page_xalloc(1, 0, 0, 0x100000000LL, mmflag);
	}

	return page;
}

/** Map a paging structure into memory.
 * @note		The calling thread should be wired.
 * @param map		Page map that the structure belongs to.
 * @param addr		Physical address of table.
 * @param mmflag	Allocation flags.
 * @return		Pointer to mapping on success, NULL on failure. */
static uint64_t *page_structure_map(page_map_t *map, phys_ptr_t addr, int mmflag) {
	assert(!IS_KERNEL_MAP(map) || !paging_inited);
	return page_phys_map(addr, PAGE_SIZE, mmflag);
}

/** Unmap a paging structure.
 * @param map		Page map that the structure belongs to.
 * @param addr		Address of mapping. */
static void page_structure_unmap(page_map_t *map, uint64_t *addr) {
	if(!IS_KERNEL_MAP(map) || !paging_inited) {
		page_phys_unmap(addr, PAGE_SIZE, false);
	}
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
 * @return		Virtual address of mapping, NULL on failure. */
static uint64_t *page_map_get_pdir(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	uint64_t *pdp, *pdir;
	phys_ptr_t page;
	int pdpe;

	assert(!(mmflag & PM_ZERO));

	/* Special handling for the kernel address space. */
	if(IS_KERNEL_MAP(map) && paging_inited) {
		/* Shouldn't be modifying anything below the top GB (the
		 * physical mapping is below, however it should not be modified
		 * after initialisation. */
		assert(virt >= 0xC0000000);
		return (uint64_t *)KERNEL_PDIR_ADDR;
	}

	/* Get the virtual address of the PDP. */
	pdp = page_structure_map(map, map->cr3, mmflag);
	if(unlikely(!pdp)) {
		return NULL;
	}

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = virt / 0x40000000;
	if(!(pdp[pdpe] & PG_PRESENT)) {
		/* Allocate a new page directory if required. */
		if(!alloc || unlikely(!(page = page_structure_alloc(mmflag | PM_ZERO)))) {
			page_structure_unmap(map, pdp);
			return NULL;
		}

		/* Map it into the PDP. */
		pdp[pdpe] = page | PG_PRESENT;

		/* Newer Intel CPUs seem to cache PDP entries and INVLPG does
		 * fuck all, completely flush the TLB if we're using this
		 * page map. */
		if(paging_inited && (x86_read_cr3() & PAGE_MASK) == map->cr3) {
			x86_write_cr3(x86_read_cr3());
		}
	}

	/* Unmap the PDP and return the page directory address. */
	pdir = page_structure_map(map, pdp[pdpe] & PHYS_PAGE_MASK, mmflag);
	page_structure_unmap(map, pdp);
	return pdir;
}

/** Get the page table containing an address.
 * @param map		Page map to get from.
 * @param virt		Virtual address to get page table for.
 * @param alloc		Whether to allocate new tables if not found.
 * @param mmflag	Allocation flags.
 * @return		Virtual address of mapping, NULL on failure. */
static uint64_t *page_map_get_ptbl(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	uint64_t *pdir, *ptbl;
	phys_ptr_t page;
	int pde;

	assert(!(mmflag & PM_ZERO));

	/* Get the page directory. */
	pdir = page_map_get_pdir(map, virt, alloc, mmflag);
	if(unlikely(!pdir)) {
		return NULL;
	}

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / LARGE_PAGE_SIZE;
	if(!(pdir[pde] & PG_PRESENT)) {
		/* Allocate a new page table if required. Allocating a page can
		 * cause page mappings to be modified (if a vmem boundary tag
		 * refill occurs), handle this possibility. */
		if(alloc) {
			page = page_structure_alloc(mmflag | PM_ZERO);
			if(pdir[pde] & PG_PRESENT) {
				if(page) {
					page_free(page, 1);
				}
			} else {
				if(unlikely(!page)) {
					page_structure_unmap(map, pdir);
					return NULL;
				}

				/* Map it into the page directory. If this is
				 * the kernel map, must invalidate the page
				 * directory mapping entry. */
				pdir[pde] = page | TABLE_MAPPING_FLAGS(map);
				if(IS_KERNEL_MAP(map) && paging_inited) {
					invlpg(KERNEL_PTBL_ADDR(virt));
					page_map_add_to_invalidate(map, KERNEL_PTBL_ADDR(virt));
				}
			}
		} else {
			page_structure_unmap(map, pdir);
			return NULL;
		}
	}

	/* Unmap the page directory and return the page table address. */
	if(IS_KERNEL_MAP(map) && paging_inited) {
		ptbl = (uint64_t *)KERNEL_PTBL_ADDR(virt);
	} else {
		ptbl = page_structure_map(map, pdir[pde] & PHYS_PAGE_MASK, mmflag);
	}
	page_structure_unmap(map, pdir);
	return ptbl;
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
 * @return		Always returns 0. */
static int tlb_invalidate_ipi(void *msg, unative_t d1, unative_t d2, unative_t d3, unative_t d4) {
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

	return 0;
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
			            0, 0, 0, IPI_SEND_SYNC) != 0) {
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
 * @return		Status code describing result of the operation. */
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
	page_structure_unmap(map, ptbl);
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
	page_structure_unmap(map, ptbl);

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
		page_structure_unmap(map, ptbl);
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
	page_structure_unmap(map, ptbl);

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
	uint64_t *pdp, *pdir, *ptbl;
	int pdpe, pde, pte;

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));
	assert(physp);

	/* This function must not use any of the helper functions, as this has
	 * to work for any virtual address - the helper functions have
	 * restrictions about which addresses can be looked up in the kernel
	 * page map. */
	pdp = page_phys_map(map->cr3, PAGE_SIZE, MM_SLEEP);
	pdpe = virt / 0x40000000;
	if(!(pdp[pdpe] & PG_PRESENT)) {
		page_phys_unmap(pdp, PAGE_SIZE, false);
		return false;
	}

	/* Find the page directory for the entry. */
	pdir = page_phys_map(pdp[pdpe] & PHYS_PAGE_MASK, PAGE_SIZE, MM_SLEEP);
	page_phys_unmap(pdp, PAGE_SIZE, false);
	pde = (virt % 0x40000000) / LARGE_PAGE_SIZE;
	if(!(pdir[pde] & PG_PRESENT)) {
		page_phys_unmap(pdir, PAGE_SIZE, false);
		return false;
	}

	/* Handle large pages. */
	if(pdir[pde] & PG_LARGE) {
		*physp = (pdir[pde] & PHYS_PAGE_MASK) + (virt % 0x200000);
		page_phys_unmap(pdir, PAGE_SIZE, false);
		return true;
	}

	/* Map in the page table. */
	ptbl = page_phys_map(pdir[pde] & PHYS_PAGE_MASK, PAGE_SIZE, MM_SLEEP);
	page_phys_unmap(pdir, PAGE_SIZE, false);
	pte = (virt % 0x200000) / PAGE_SIZE;
	if(!(ptbl[pte] & PG_PRESENT)) {
		page_phys_unmap(ptbl, PAGE_SIZE, false);
		return false;
	}

	*physp = ptbl[pte] & PHYS_PAGE_MASK;
	page_phys_unmap(ptbl, PAGE_SIZE, false);
	return true;
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
	uint64_t *kpdp, *pdp;

	mutex_init(&map->lock, "page_map_lock", MUTEX_RECURSIVE);
	map->invalidate_count = 0;
	map->cr3 = page_structure_alloc(mmflag | PM_ZERO);
	if(!map->cr3) {
		return STATUS_NO_MEMORY;
	}

	if(!IS_KERNEL_MAP(map)) {
		/* Duplicate the kernel mappings. */
		kpdp = page_structure_map(map, kernel_page_map.cr3, mmflag);
		if(!kpdp) {
			page_free(map->cr3, 1);
			return STATUS_NO_MEMORY;
		}
		pdp = page_structure_map(map, map->cr3, mmflag);
		if(!pdp) {
			page_structure_unmap(map, kpdp);
			page_free(map->cr3, 1);
			return STATUS_NO_MEMORY;
		}

		pdp[2] = kpdp[2] & ~PG_ACCESSED;
		pdp[3] = kpdp[3] & ~PG_ACCESSED;

		page_structure_unmap(map, pdp);
		page_structure_unmap(map, kpdp);
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
	uint64_t *pdp, *pdir;
	int i, j;

	assert(!IS_KERNEL_MAP(map));

	/* Free all structures in the bottom half of the PDP (user memory). */
	pdp = page_structure_map(map, map->cr3, MM_SLEEP);
	for(i = 0; i < 2; i++) {
		if(!(pdp[i] & PG_PRESENT)) {
			continue;
		}

		pdir = page_structure_map(map, pdp[i] & PHYS_PAGE_MASK, MM_SLEEP);
		for(j = 0; j < 512; j++) {
			if(!(pdir[j] & PG_PRESENT)) {
				continue;
			}

			if(!(pdir[j] & PG_LARGE)) {
				page_free(pdir[j] & PHYS_PAGE_MASK, 1);
			}
		}
		page_structure_unmap(map, pdir);

		page_free(pdp[i] & PHYS_PAGE_MASK, 1);
	}
	page_structure_unmap(map, pdp);

	page_free(map->cr3, 1);
}

/** Map physical memory into the kernel address space.
 * @note		The range does not need to be page-aligned.
 * @param addr		Physical address to map.
 * @param size		Size of range to map.
 * @param mmflag	Allocation behaviour flags.
 * @return		Address of mapping or NULL on failure. */
void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag) {
	phys_ptr_t base, end;
	char *ret;

	if(!size) {
		return NULL;
	}

	if(paging_inited) {
		if(addr < KERNEL_PMAP_SIZE && (addr + size) <= KERNEL_PMAP_SIZE) {
			return (void *)((ptr_t)addr + KERNEL_PMAP_BASE);
		} else {
			/* Work out the page that the address starts on and the
			 * actual size of the mapping we need to make. */
			base = ROUND_DOWN(addr, PAGE_SIZE);
			end = ROUND_UP(addr + size, PAGE_SIZE);

			if(!(ret = kheap_map_range(base, end - base, mmflag))) {
				return NULL;
			}

			return ret + (addr - base);
		}
	} else {
		/* During boot there is a 1GB identity mapping. */
		assert(addr < 0x40000000);
		assert((addr + size) <= 0x40000000);
		return (void *)((ptr_t)addr);
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
	ptr_t base, end;

	if((ptr_t)addr >= (KERNEL_PMAP_BASE + KERNEL_PMAP_SIZE)) {
		base = ROUND_DOWN((ptr_t)addr, PAGE_SIZE);
		end = ROUND_UP((ptr_t)addr + size, PAGE_SIZE);

		kheap_unmap_range((void *)base, end - base, shared);
	}
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

/** Perform IA32 paging initialisation.
 * @param args		Kernel arguments pointer. */
void __init_text page_arch_init(kernel_args_t *args) {
	uint64_t *pdp, *bpdp, *pdir;
	phys_ptr_t i;
	int pde;

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

	/* Create a 1GB physical mapping. */
	pdir = page_map_get_pdir(&kernel_page_map, KERNEL_PMAP_BASE, true, MM_FATAL);
	for(i = 0; i < KERNEL_PMAP_SIZE; i += LARGE_PAGE_SIZE) {
		pdir[i / LARGE_PAGE_SIZE] = i | PG_PRESENT | PG_WRITE | PG_GLOBAL | PG_LARGE;
	}
	page_structure_unmap(&kernel_page_map, pdir);

	/* Add the fractal mapping for the kernel page table. */
	pdp = page_phys_map(kernel_page_map.cr3, PAGE_SIZE, MM_FATAL);
	pdir = page_phys_map(pdp[3] & PHYS_PAGE_MASK, PAGE_SIZE, MM_FATAL);
	pde = (KERNEL_PTBL_BASE % 0x40000000) / LARGE_PAGE_SIZE;
	pdir[pde] = (pdp[3] & PHYS_PAGE_MASK) | PG_PRESENT | PG_WRITE;
	page_phys_unmap(pdir, PAGE_SIZE, true);

	/* The temporary identity mapping is still required as all the CPUs'
	 * stack pointers are in it, and the kernel arguments pointer points to
	 * it. Use the structures from the bootloader rather than just using
	 * the new physical map page directory because the new one has the
	 * global flag set on all pages, which makes invalidating the TLB
	 * entries difficult when removing the mapping. */
	bpdp = page_phys_map(x86_read_cr3() & PAGE_MASK, PAGE_SIZE, MM_FATAL);
	pdp[0] = bpdp[0];
	page_phys_unmap(bpdp, PAGE_SIZE, true);
	page_phys_unmap(pdp, PAGE_SIZE, true);

	page_map_unlock(&kernel_page_map);
	dprintf("page: initialised kernel page map (pdp: 0x%" PRIpp ")\n",
	        kernel_page_map.cr3);

	/* Switch to the kernel page map. */
	page_map_switch(&kernel_page_map);

	/* The physical map area can now be used. */
	paging_inited = true;
}

/** TLB flush IPI handler.
 * @return		Always returns 0. */
static int tlb_flush_ipi(void *msg, unative_t d1, unative_t d2, unative_t d3, unative_t d4) {
	x86_write_cr3(x86_read_cr3());
	return 0;
}

/** Perform late IA32 paging initialisation. */
void page_arch_late_init(void) {
	uint64_t *pdp;

	/* All of the CPUs have been booted and have new stacks, and the kernel
	 * arguments are no longer required. Remove the temporary identity
	 * mapping and flush the TLB on all CPUs. */
	pdp = page_phys_map(kernel_page_map.cr3, PAGE_SIZE, MM_FATAL);
	pdp[0] = 0;
	page_phys_unmap(pdp, PAGE_SIZE, true);
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
