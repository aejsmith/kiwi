/*
 * Copyright (C) 2009 Alex Smith
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

#include <arch/barrier.h>
#include <arch/features.h>
#include <arch/memmap.h>
#include <arch/sysreg.h>

#include <cpu/cpu.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kheap.h>
#include <mm/page.h>
#include <mm/tlb.h>
#include <mm/vm.h>

#include <proc/thread.h>

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
	assert(map->user || !paging_inited);
	thread_wire(curr_thread);
	return page_phys_map(addr, PAGE_SIZE, mmflag);
}

/** Unmap a paging structure.
 * @param map		Page map that the structure belongs to.
 * @param addr		Address of mapping. */
static void page_structure_unmap(page_map_t *map, uint64_t *addr) {
	if(map->user || !paging_inited) {
		page_phys_unmap(addr, PAGE_SIZE, false);
		thread_unwire(curr_thread);
	}
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
	if(!map->user && paging_inited) {
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
		if(paging_inited && (sysreg_cr3_read() & PAGE_MASK) == map->cr3) {
			sysreg_cr3_write(sysreg_cr3_read());
		}
	}

	/* Unmap the PDP and return the page directory address. */
	pdir = page_structure_map(map, pdp[pdpe] & PAGE_MASK, mmflag);
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
		/* Allocate a new page table if required. */
		if(!alloc || unlikely(!(page = page_structure_alloc(mmflag | PM_ZERO)))) {
			page_structure_unmap(map, pdir);
			return NULL;
		}

		/* Map it into the page directory. */
		pdir[pde] = page | PG_PRESENT | PG_WRITE | ((map->user) ? PG_USER : 0);
	}

	/* Unmap the page directory and return the page table address. */
	if(!map->user && paging_inited) {
		ptbl = (uint64_t *)KERNEL_PTBL_ADDR(virt);
	} else {
		ptbl = page_structure_map(map, pdir[pde] & PAGE_MASK, mmflag);
	}
	page_structure_unmap(map, pdir);
	return ptbl;
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

	mutex_lock(&map->lock);

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
	page_structure_unmap(map, pdir);
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

	mutex_lock(&map->lock);

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
	page_structure_unmap(map, ptbl);
	mutex_unlock(&map->lock);
	return 0;
}

/** Unmap a page.
 * @param map		Page map to unmap from in.
 * @param virt		Virtual address to unmap.
 * @param physp		Where to store physical address that was mapped.
 * @return		Whether the address was mapped. */
bool page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	phys_ptr_t paddr;
	vm_page_t *page;
	uint64_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));

	mutex_lock(&map->lock);

	/* Find the page table for the entry. */
	pte = (virt % LARGE_PAGE_SIZE) / PAGE_SIZE;
	if(!(ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP))) {
		mutex_unlock(&map->lock);
		return false;
	} else if(!ptbl[pte] & PG_PRESENT) {
		page_structure_unmap(map, ptbl);
		mutex_unlock(&map->lock);
		return false;
	}

	paddr = ptbl[pte] & PAGE_MASK;

	/* If the entry is dirty, set the modified flag on the page. */
	if(ptbl[pte] & PG_DIRTY && (page = vm_page_lookup(paddr))) {
		page->modified = true;
	}

	/* Clear the entry. */
	ptbl[pte] = 0;
	memory_barrier();
	page_structure_unmap(map, ptbl);

	if(physp) {
		*physp = paddr;
	}
	mutex_unlock(&map->lock);
	return true;
}

/** Find the page a virtual address is mapped to.
 * @param map		Page map to find in.
 * @param virt		Virtual address to find.
 * @param physp		Where to store physical address.
 * @return		Whether the virtual address was mapped. */
bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	bool ret = false;
	uint64_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(physp);

	mutex_lock(&map->lock);

	/* Find the page table for the entry. */
	if((ptbl = page_map_get_ptbl(map, virt, false, MM_SLEEP))) {
		pte = (virt % 0x200000) / PAGE_SIZE;
		if(ptbl[pte] & PG_PRESENT) {
			*physp = ptbl[pte] & PAGE_MASK;
			ret = true;
		}
	}

	page_structure_unmap(map, ptbl);
	mutex_unlock(&map->lock);
	return ret;
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

	mutex_lock(&map->lock);

	for(i = start; i < end; i += PAGE_SIZE) {
		pte = (i % LARGE_PAGE_SIZE) / PAGE_SIZE;

		if(!(ptbl = page_map_get_ptbl(map, i, false, MM_SLEEP))) {
			continue;
		} else if(!(ptbl[pte] & PG_PRESENT)) {
			page_structure_unmap(map, ptbl);
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
		memory_barrier();
		page_structure_unmap(map, ptbl);
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
	uint64_t *kpdp, *pdp;

	mutex_init(&map->lock, "page_map_lock", MUTEX_RECURSIVE);
	map->user = true;

	if(!(map->cr3 = page_structure_alloc(mmflag | PM_ZERO))) {
		return -ERR_NO_MEMORY;
	}

	/* Duplicate the kernel mappings. */
	if(!(kpdp = page_structure_map(map, kernel_page_map.cr3, mmflag))) {
		page_free(map->cr3, 1);
		return -ERR_NO_MEMORY;
	} else if(!(pdp = page_structure_map(map, map->cr3, mmflag))) {
		page_structure_unmap(map, kpdp);
		page_free(map->cr3, 1);
		return -ERR_NO_MEMORY;
	}

	pdp[2] = kpdp[2] & ~PG_ACCESSED;
	pdp[3] = kpdp[3] & ~PG_ACCESSED;

	page_structure_unmap(map, pdp);
	page_structure_unmap(map, kpdp);
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
	uint64_t *pdp, *pdir;
	int i, j;

	/* Free all structures in the bottom half of the PDP (user memory). */
	pdp = page_structure_map(map, map->cr3, MM_SLEEP);
	for(i = 0; i < 2; i++) {
		if(!(pdp[i] & PG_PRESENT)) {
			continue;
		}

		pdir = page_structure_map(map, pdp[i] & PAGE_MASK, MM_SLEEP);
		for(j = 0; j < 512; j++) {
			if(!(pdir[j] & PG_PRESENT)) {
				continue;
			}

			if(!(pdir[j] & PG_LARGE)) {
				page_free(pdir[j] & PAGE_MASK, 1);
			}
		}
		page_structure_unmap(map, pdir);

		page_free(pdp[i] & PAGE_MASK, 1);
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
	mutex_init(&kernel_page_map.lock, "kernel_page_map_lock", MUTEX_RECURSIVE);
	kernel_page_map.cr3 = page_structure_alloc(MM_FATAL | PM_ZERO);
	kernel_page_map.user = false;

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
	for(i = 0; i < KERNEL_PMAP_SIZE; i += LARGE_PAGE_SIZE) {
		page_map_insert_large(&kernel_page_map, i + KERNEL_PMAP_BASE,
		                      i, true, true, MM_FATAL);
	}

	/* Add the fractal mapping for the kernel page table. */
	pdp = page_phys_map(kernel_page_map.cr3, PAGE_SIZE, MM_FATAL);
	pdir = page_phys_map(pdp[3] & PAGE_MASK, PAGE_SIZE, MM_FATAL);
	pde = (KERNEL_PTBL_BASE % 0x40000000) / LARGE_PAGE_SIZE;
	pdir[pde] = (pdp[3] & PAGE_MASK) | PG_PRESENT | PG_WRITE;
	page_phys_unmap(pdir, PAGE_SIZE, true);

	/* The temporary identity mapping is still required as all the CPUs'
	 * stack pointers are in it, and the kernel arguments pointer points to
	 * it. Use the structures from the bootloader rather than just using
	 * the new physical map page directory because the new one has the
	 * global flag set on all pages, which makes invalidating the TLB
	 * entries difficult when removing the mapping. */
	bpdp = page_phys_map(sysreg_cr3_read() & PAGE_MASK, PAGE_SIZE, MM_FATAL);
	pdp[0] = bpdp[0];
	page_phys_unmap(bpdp, PAGE_SIZE, true);
	page_phys_unmap(pdp, PAGE_SIZE, true);

	dprintf("page: initialised kernel page map (pdp: 0x%" PRIpp ")\n",
	        kernel_page_map.cr3);

	/* Switch to the kernel page map. */
	page_map_switch(&kernel_page_map);

	/* The physical map area can now be used. */
	paging_inited = true;
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
	tlb_invalidate(NULL, 0, 0);
}
