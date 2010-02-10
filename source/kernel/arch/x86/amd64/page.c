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
#include <arch/features.h>
#include <arch/memmap.h>
#include <arch/sysreg.h>

#include <cpu/cpu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>

#include <assert.h>
#include <console.h>
#include <errors.h>
#include <fatal.h>
#include <kargs.h>

#if CONFIG_PAGE_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern char __text_start[], __text_end[];
extern char __init_start[], __init_end[];
extern char __rodata_start[], __rodata_end[];
extern char __data_start[], __bss_end[];

/** Kernel page map. */
page_map_t g_kernel_page_map;

/** Whether the kernel page map has been initialised. */
static bool g_paging_inited = false;

/** Allocate a paging structure.
 * @param mmflag	Allocation flags. PM_ZERO will be added.
 * @return		Address of structure on success, 0 on failure. */
static phys_ptr_t page_structure_alloc(int mmflag) {
	if(g_paging_inited) {
		return page_alloc(1, mmflag | PM_ZERO);
	} else {
		/* During initialisation we only have 1GB of physical mapping. */
		return page_xalloc(1, 0, 0, 0, 0, 0x40000000, mmflag | PM_ZERO);
	}
}

/** Get the virtual address of a page structure.
 * @param addr		Address of structure.
 * @return		Pointer to mapping. */
static uint64_t *page_structure_map(phys_ptr_t addr) {
	/* Our page_phys_map() implementation never fails. */
	return page_phys_map(addr, PAGE_SIZE, MM_FATAL);
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
				if(!page) {
					return NULL;
				}

				/* Map it into the PML4. */
				pml4[pml4e] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return NULL;
		}
	}

	/* Get the PDP from the PML4. */
	pdp = page_structure_map(pml4[pml4e] & PAGE_MASK);

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
				if(!page) {
					return NULL;
				}

				/* Map it into the PDP. */
				pdp[pdpe] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return NULL;
		}
	}

	/* Return the page directory address. */
	return page_structure_map(pdp[pdpe] & PAGE_MASK);
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
				if(!page) {
					return NULL;
				}

				/* Map it into the page directory. */
				pdir[pde] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
			}
		} else {
			return NULL;
		}
	}

	/* If this function is being used it should not be a large page. */
	assert(!(pdir[pde] & PG_LARGE));
	return page_structure_map(pdir[pde] & PAGE_MASK);
}

/** Map a large page into a page map.
 * @param map		Page map to map into.
 * @param virt		Virtual address to map (multiple of LARGE_PAGE_SIZE).
 * @param phys		Page to map to (multiple of LARGE_PAGE_SIZE).
 * @param write		Whether to make the mapping writable.
 * @param exec		Whether to make the mapping executable.
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
static int page_map_insert_large(page_map_t *map, ptr_t virt, phys_ptr_t phys,
                                 bool write, bool exec, int mmflag) {
	uint64_t *pdir, flags;
	int pde;

	assert(!(virt % LARGE_PAGE_SIZE));
	assert(!(phys % LARGE_PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Find the page directory for the entry. */
	if(!(pdir = page_map_get_pdir(map, virt, true, mmflag))) {
		mutex_unlock(&map->lock);
		return -ERR_NO_MEMORY;
	}

	/* Check that the mapping doesn't already exist. */
	pde = (virt % 0x40000000) / LARGE_PAGE_SIZE;
	if(pdir[pde] & PG_PRESENT) {
		fatal("Mapping %p which is already mapped", virt);
	}

	/* Determine the flags to map with. Kernel mappings are created with
	 * the global flag. */
	flags = PG_PRESENT | ((map->user) ? PG_USER : PG_GLOBAL) | PG_LARGE;
	if(write) {
		flags |= PG_WRITE;
	}
#if CONFIG_X86_NX
	if(!exec && CPU_HAS_XD(curr_cpu)) {
		flags |= PG_NOEXEC;
	}
#endif
	/* Create the mapping. */
	pdir[pde] = phys | flags;
	memory_barrier();
	mutex_unlock(&map->lock);
	return 0;
}

/** Map a page into a page map.
 * @param map		Page map to map into.
 * @param virt		Virtual address to map.
 * @param phys		Page to map to.
 * @param write		Whether to make the mapping writable.
 * @param exec		Whether to make the mapping executable.
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. */
int page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, bool write,
                    bool exec, int mmflag) {
	uint64_t *ptbl, flags;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Find the page table for the entry. */
	if(!(ptbl = page_map_get_ptbl(map, virt, true, mmflag))) {
		mutex_unlock(&map->lock);
		return -ERR_NO_MEMORY;
	}

	/* Check that the mapping doesn't already exist. */
	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	if(ptbl[pte] & PG_PRESENT) {
		fatal("Mapping %p which is already mapped", virt);
	}

	/* Determine the flags to map with. Kernel mappings are created with
	 * the global flag. */
	flags = PG_PRESENT | ((map->user) ? PG_USER : PG_GLOBAL);
	if(write) {
		flags |= PG_WRITE;
	}
#if CONFIG_X86_NX
	if(!exec && CPU_HAS_XD(curr_cpu)) {
		flags |= PG_NOEXEC;
	}
#endif
	/* Create the mapping. */
	ptbl[pte] = phys | flags;
	memory_barrier();
	mutex_unlock(&map->lock);
	return 0;
}

/** Unmap a page.
 * @param map		Page map to unmap from in.
 * @param virt		Virtual address to unmap.
 * @param physp		Where to store physical address that was mapped.
 * @return		Whether the address was mapped. */
bool page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	uint64_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Find the page table for the entry. */
	if(!(ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP))) {
		mutex_unlock(&map->lock);
		return -ERR_NOT_FOUND;
	}

	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	if(ptbl[pte] & PG_PRESENT) {
		/* Store the physical address if required. */
		if(physp) {
			*physp = ptbl[pte] & PAGE_MASK;
		}

		/* Clear the entry. */
		ptbl[pte] = 0;
		memory_barrier();
		mutex_unlock(&map->lock);
		return 0;
	} else {
		mutex_unlock(&map->lock);
		return -ERR_NOT_FOUND;
	}
}

/** Find the page a virtual address is mapped to.
 * @param map		Page map to find in.
 * @param virt		Virtual address to find.
 * @param physp		Where to store physical address.
 * @return		Whether the virtual address was mapped. */
bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	uint64_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(physp);

	mutex_lock(&map->lock, 0);

	/* Find the page table for the entry. */
	if((ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP))) {
		pte = (virt % 0x200000) / PAGE_SIZE;
		if(ptbl[pte] & PG_PRESENT) {
			*physp = ptbl[pte] & PAGE_MASK;
			mutex_unlock(&map->lock);
			return true;
		}
	}

	mutex_unlock(&map->lock);
	return false;
}

/** Modify the protection flags on a range.
 *
 * Modifies the protection flags on a range in a page map. Any entries that are
 * not currently mapped will be ignored.
 *
 * @param map		Page map to modify.
 * @param start		Start of range to modify.
 * @param end		End of range to modify.
 * @param write		Whether the range should be writable.
 * @param exec		Whether the range should be executable.
 */
void page_map_remap(page_map_t *map, ptr_t start, ptr_t end, bool write, bool exec) {
	uint64_t *ptbl, flags;
	ptr_t i;
	int pte;

	assert(!(start % PAGE_SIZE));
	assert(!(end % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	for(i = start; i < end; i += PAGE_SIZE) {
		pte = (i % LARGE_PAGE_SIZE) / PAGE_SIZE;

		if(!(ptbl = page_map_get_ptbl(map, i, false, MM_SLEEP))) {
			continue;
		} else if(!(ptbl[pte] & PG_PRESENT)) {
			continue;
		}

		/* Work out the new flags to set. */
		flags = 0;
		if(write) {
			flags |= PG_WRITE;
		}
#if CONFIG_X86_NX
		if(!exec && CPU_HAS_XD(curr_cpu)) {
			flags |= PG_NOEXEC;
		}
#endif
		/* Clear out original flags, and set the new flags. */
		ptbl[pte] = (ptbl[pte] & ~(PG_WRITE | PG_NOEXEC)) | flags;
	}

	mutex_unlock(&map->lock);
}

/** Switch to a page map.
 * @param map		Page map to switch to. */
void page_map_switch(page_map_t *map) {
	sysreg_cr3_write(map->cr3);
}

/** Initialise a page map.
 * @param map		Page map to initialise.
 * @param mmflag	Allocation flags.
 * @return		0 on success, negative error code on failure. Failure
 *			can only occur if MM_SLEEP is not specified. */
int page_map_init(page_map_t *map, int mmflag) {
	uint64_t *kpml4, *pml4;

	mutex_init(&map->lock, "page_map_lock", MUTEX_RECURSIVE);
	map->cr3 = page_structure_alloc(mmflag);
	map->user = true;

	/* Get the kernel mappings into the new PML4. */
	kpml4 = page_structure_map(g_kernel_page_map.cr3);
	pml4 = page_structure_map(map->cr3);
	pml4[511] = kpml4[511] & ~PG_ACCESSED;
	return 0;
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

	assert(map->user);

	/* Free all structures in the bottom half of the PML4 (user memory). */
	pml4 = page_structure_map(map->cr3);
	for(i = 0; i < 256; i++) {
		if(!(pml4[i] & PG_PRESENT)) {
			continue;
		}

		pdp = page_structure_map(pml4[i] & PAGE_MASK);
		for(j = 0; j < 512; j++) {
			if(!(pdp[j] & PG_PRESENT)) {
				continue;
			}

			pdir = page_structure_map(pdp[j] & PAGE_MASK);
			for(k = 0; k < 512; k++) {
				if(!(pdir[k] & PG_PRESENT)) {
					continue;
				}

				if(!(pdir[k] & PG_LARGE)) {
					page_free(pdir[k] & PAGE_MASK, 1);
				}
			}

			page_free(pdp[j] & PAGE_MASK, 1);
		}

		page_free(pml4[i] & PAGE_MASK, 1);
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
	if(g_paging_inited) {
		return (void *)(KERNEL_PMAP_BASE + addr);
	} else {
		/* During boot there is a 1GB identity mapping. */
		assert(addr < 0x40000000);
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
		page_map_insert(&g_kernel_page_map, start + i, phys + i, write, exec, MM_FATAL);
	}

	dprintf("page: created kernel mapping [%p,%p) to [0x%" PRIpp ",0x%" PRIpp ") (%d %d)\n",
	        start, end, phys, phys + (end - start), write, exec);
}

/** Perform AMD64 paging initialisation.
 * @param args		Kernel arguments pointer. */
void __init_text page_arch_init(kernel_args_t *args) {
	uint64_t *pml4;
	phys_ptr_t i;
#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		dprintf("page: CPU supports NX/XD, enabling...\n");
		sysreg_msr_write(SYSREG_MSR_EFER, sysreg_msr_read(SYSREG_MSR_EFER) | SYSREG_EFER_NXE);
	}
#endif
	/* Initialise the kernel page map structure. */
	mutex_init(&g_kernel_page_map.lock, "kernel_page_map_lock", MUTEX_RECURSIVE);
	g_kernel_page_map.cr3 = page_structure_alloc(MM_FATAL);
	g_kernel_page_map.user = false;

	/* Map the kernel in. The following mappings are made:
	 *  .text      - R/X
	 *  .init      - R/W/X
	 *  .rodata    - R
	 *  .data/.bss - R/W */
	page_map_kernel_range(args, ROUND_DOWN((ptr_t)__text_start, PAGE_SIZE), (ptr_t)__text_end, false, true);
	page_map_kernel_range(args, (ptr_t)__init_start, (ptr_t)__init_end, true, true);
	page_map_kernel_range(args, (ptr_t)__rodata_start, (ptr_t)__rodata_end, false, false);
	page_map_kernel_range(args, (ptr_t)__data_start, (ptr_t)__bss_end, true, false);

	/* Create 4GB of physical mapping for now. FIXME. */
	for(i = 0; i < 0x100000000; i += LARGE_PAGE_SIZE) {
		page_map_insert_large(&g_kernel_page_map, i + KERNEL_PMAP_BASE,
		                      i, true, true, MM_FATAL);
	}

	/* The temporary identity mapping is still required as all the CPUs'
	 * stack pointers are in it, and the kernel arguments pointer points to
	 * it. Just point the first PML4 entry to the kernel PDP. */
	pml4 = page_structure_map(g_kernel_page_map.cr3);
	pml4[0] = pml4[511];

	dprintf("page: initialised kernel page map (pml4: 0x%" PRIpp ")\n",
	        g_kernel_page_map.cr3);

	/* Switch to the kernel page map. */
	page_map_switch(&g_kernel_page_map);

	/* The physical map area can now be used. */
	g_paging_inited = true;
}
