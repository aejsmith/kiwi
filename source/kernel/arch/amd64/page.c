/* Kiwi AMD64 paging functions
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
 * @brief		AMD64 paging functions.
 */

#include <arch/asm.h>
#include <arch/barrier.h>
#include <arch/defs.h>
#include <arch/features.h>
#include <arch/mem.h>

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/page.h>
#include <mm/pmm.h>

#include <assert.h>
#include <fatal.h>

/** Function declarations. */
extern void page_late_init(void);

/** Kernel paging structures. */
extern uint64_t __boot_pml4[];
extern pte_t __kernel_pdp[];

/** Symbols defined by the linker script. */
extern char __text_start[], __text_end[], __rodata_start[], __rodata_end[];
extern char __bss_end[], __end[];

/*
 * Page map functions.
 */

/** Helper macro to clear a page table entry. */
#define SIMPLE_CLEAR_PTE(ptbl, i)	(((pte_simple_t *)(ptbl))[(i)] = 0)

/** Helper macro to set a page table entry. */
#define SIMPLE_SET_PTE(ptbl, i, v)	(((pte_simple_t *)(ptbl))[(i)] = (v))

/** Kernel page map. */
page_map_t kernel_page_map;

/** Get the page table containing an address.
 * @param map		Page map to get from.
 * @param virt		Address to get page table for.
 * @param alloc		Whether to allocate structures if not found.
 * @param mmflag	Allocation flags.
 * @return		Virtual address of page table. */
static pte_t *page_map_get_ptbl(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	pte_t *pml4, *pdp, *pdir, *ptbl;
	int pml4e, pdpe, pde;
	phys_ptr_t page;

	/* Get the virtual address of the PML4. Note that unmapping is not
	 * necessary because of our page_phys_map() implementation. */
	pml4 = page_phys_map(map->pml4, PAGE_SIZE, mmflag);

	/* Get the page directory pointer number. A PDP covers 512GB. */
	pml4e = (virt & 0x0000FFFFFFFFF000) / 0x8000000000;
	if(!pml4[pml4e].present) {
		/* Allocate a new PDP if required. Safe to use PM_ZERO because
		 * our implementation of page_phys_map() doesn't touch the
		 * heap. */
		if(!alloc || !(page = pmm_alloc(1, mmflag | PM_ZERO))) {
			return NULL;
		}

		/* Map it into the PML4. */
		pdp = page_phys_map(page, PAGE_SIZE, mmflag);
		pml4[pml4e].address = page >> PAGE_WIDTH;
		pml4[pml4e].writable = 1;
		pml4[pml4e].user = map->user;
		pml4[pml4e].present = 1;
	} else {
		pdp = page_phys_map((phys_ptr_t)pml4[pml4e].address << PAGE_WIDTH, PAGE_SIZE, mmflag);
	}

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!pdp[pdpe].present) {
		/* Allocate a new page directory if required. */
		if(!alloc || !(page = pmm_alloc(1, mmflag | PM_ZERO))) {
			return NULL;
		}

		/* Map it into the PDP. */
		pdir = page_phys_map(page, PAGE_SIZE, mmflag);
		pdp[pdpe].address = page >> PAGE_WIDTH;
		pdp[pdpe].writable = 1;
		pdp[pdpe].user = map->user;
		pdp[pdpe].present = 1;
	} else {
		pdir = page_phys_map((phys_ptr_t)pdp[pdpe].address << PAGE_WIDTH, PAGE_SIZE, mmflag);
	}

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / 0x200000;
	if(!pdir[pde].present) {
		/* Allocate a new page table if required. */
		if(!alloc || !(page = pmm_alloc(1, mmflag | PM_ZERO))) {
			return NULL;
		}

		/* Map it into the page directory. */
		ptbl = page_phys_map(page, PAGE_SIZE, mmflag);
		pdir[pde].address = page >> PAGE_WIDTH;
		pdir[pde].writable = 1;
		pdir[pde].user = map->user;
		pdir[pde].present = 1;
	} else {
		assert(!pdir[pde].large);
		ptbl = page_phys_map((phys_ptr_t)pdir[pde].address << PAGE_WIDTH, PAGE_SIZE, mmflag);
	}

	return ptbl;
}

/** Insert a mapping in a page map.
 *
 * Maps a virtual address to a physical address with the given protection
 * settings in a page map.
 *
 * @param map		Page map to insert in.
 * @param virt		Virtual address to map.
 * @param phys		Physical address to map to.
 * @param prot		Protection flags.
 * @param mmflag	Page allocation flags.
 *
 * @return		True if operation succeeded, false if not.
 */
bool page_map_insert(page_map_t *map, ptr_t virt, phys_ptr_t phys, int prot, int mmflag) {
	pte_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Check that we can map here. */
	if(virt < map->first || virt > map->last) {
		fatal("Map on 0x%p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, true, mmflag);
	if(ptbl == NULL) {
		mutex_unlock(&map->lock);
		return false;
	}

	pte = (virt % 0x200000) / PAGE_SIZE;
	if(ptbl[pte].present) {
		fatal("Mapping 0x%p which is already mapped", virt);
	} else {
		ptbl[pte].address = phys >> PAGE_WIDTH;
		ptbl[pte].writable = (prot & PAGE_MAP_WRITE) ? 1 : 0;
		ptbl[pte].user = map->user;
		ptbl[pte].global = !map->user;
#if CONFIG_X86_NX
		ptbl[pte].noexec = (!(prot & PAGE_MAP_EXEC) && CPU_HAS_XD(curr_cpu)) ? 1 : 0;
#endif
		ptbl[pte].present = 1;

		/* TODO: proper TLB handling. */
		invlpg(virt);

		mutex_unlock(&map->lock);
		return true;
	}
}

/** Remove a mapping from a page map.
 *
 * Removes the mapping at a virtual address from a page map.
 *
 * @param map		Page map to unmap from.
 * @param virt		Virtual address to unmap.
 * @param physp		Where to store mapping's physical address prior to
 *			unmapping (can be NULL).
 *
 * @return		True if operation succeeded, false if not.
 */
bool page_map_remove(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	bool ret = false;
	pte_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));

	mutex_lock(&map->lock, 0);

	/* Check that we can map here. */
	if(virt < map->first || virt > map->last) {
		fatal("Unmap on 0x%p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, false, 0);
	if(ptbl == NULL) {
		mutex_unlock(&map->lock);
		return false;
	}

	pte = (virt % 0x200000) / PAGE_SIZE;
	if(ptbl[pte].present) {
		/* Store the physical address if required. */
		if(physp != NULL) {
			*physp = ptbl[pte].address << PAGE_WIDTH;
		}

		/* Clear the entry. */
		SIMPLE_CLEAR_PTE(ptbl, pte);

		/* TODO: proper TLB handling. */
		invlpg(virt);

		ret = true;
	}

	mutex_unlock(&map->lock);
	return ret;
}

/** Get the value of a mapping in a page map.
 *
 * Gets the physical address, if any, that a virtual address is mapped to in
 * a page map.
 *
 * @param map		Page map to lookup in.
 * @param virt		Address to find.
 * @param physp		Where to store mapping's value.
 *
 * @return		True if mapping is present, false if not.
 */
bool page_map_find(page_map_t *map, ptr_t virt, phys_ptr_t *physp) {
	bool ret = false;
	pte_t *ptbl;
	int pte;

	assert(!(virt % PAGE_SIZE));
	assert(physp);

	mutex_lock(&map->lock, 0);

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, false, 0);
	if(ptbl != NULL) {
		pte = (virt % 0x200000) / PAGE_SIZE;
		if(ptbl[pte].present) {
			*physp = ptbl[pte].address << PAGE_WIDTH;
			ret = true;
		}
	}

	mutex_unlock(&map->lock);
	return ret;
}

/** Switch to a different page map.
 *
 * Switches to a different page map.
 *
 * @param map		Page map to switch to.
 */
void page_map_switch(page_map_t *map) {
	write_cr3(map->pml4);
}

/** Initialize a page map structure.
 *
 * Initializes a userspace page map structure.
 *
 * @param map		Page map to initialize.
 *
 * @return		0 on success, negative error code on failure.
 */
int page_map_init(page_map_t *map) {
	pte_t *pml4;

	mutex_init(&map->lock, "page_map_lock");
	map->pml4 = pmm_alloc(1, MM_SLEEP | PM_ZERO);
	map->user = true;
	map->first = USPACE_BASE;
	map->last = (USPACE_BASE + USPACE_SIZE) - PAGE_SIZE;

	/* Get the kernel mappings into the new PML4. */
	pml4 = page_phys_map(map->pml4, PAGE_SIZE, MM_SLEEP);
	pml4[511].present = 1;
	pml4[511].writable = 1;
	pml4[511].user = 0;
	pml4[511].address = (KA2PA(__kernel_pdp)) >> PAGE_WIDTH;

	return 0;
}

/** Destroy a page map.
 *
 * Destroys all mappings in a page map and frees up anything it has allocated.
 *
 * @todo		Implement this properly.
 *
 * @param map		Page map to destroy.
 */
void page_map_destroy(page_map_t *map) {
	pmm_free(map->pml4, 1);
}

/*
 * Physical memory access functions.
 */

/** Map physical memory into the kernel address space.
 *
 * Maps a range of physical memory into the kernel's address space. The
 * range does not have to be page-aligned. When the memory is no longer needed,
 * the mapping should be removed with page_phys_unmap().
 *
 * @param addr		Base of range to map.
 * @param size		Size of range to map.
 * @param mmflag	Allocation flags.
 *
 * @return		Virtual address of mapping.
 */
void *page_phys_map(phys_ptr_t addr, size_t size, int mmflag) {
	if(size == 0) {
		return NULL;
	}
	return (void *)((ptr_t)addr + KERNEL_PMAP_BASE);
}

/** Unmap physical memory.
 *
 * Unmaps a range of physical memory previously mapped with page_phys_map().
 *
 * @param addr		Virtual address returned from page_phys_map().
 * @param size		Size of original mapping.
 */
void page_phys_unmap(void *addr, size_t size) {
	/* Nothing happens. */
}

/*
 * Paging initialization functions.
 */

/** Convert a large page to a page table if necessary.
 * @param virt		Virtual address to check. */
static void page_large_to_ptbl(ptr_t virt) {
	pte_t *pdir, *ptbl;
	int pdpe, pde, i;
	phys_ptr_t page;

	pdpe = (virt % 0x8000000000) / 0x40000000;
	if(!__kernel_pdp[pdpe].present) {
		return;
	}

	pdir = page_phys_map((phys_ptr_t)__kernel_pdp[pdpe].address << PAGE_WIDTH, PAGE_SIZE, MM_FATAL);

	pde = (virt % 0x40000000) / 0x200000;
	if(pdir[pde].large) {
		page = pmm_alloc(1, MM_FATAL);
		ptbl = page_phys_map(page, PAGE_SIZE, MM_FATAL);
		memset(ptbl, 0, PAGE_SIZE);

		/* Set pages and copy all flags from the PDE. */
		for(i = 0; i < 512; i++) {
			ptbl[i].present = 1;
			ptbl[i].writable = pdir[pde].writable;
			ptbl[i].user = 0;
			ptbl[i].global = pdir[pde].global;
			ptbl[i].address = pdir[pde].address + i;
			ptbl[i].noexec = pdir[pde].noexec;
		}

		/* Replace the large page in the page directory. */
		SIMPLE_SET_PTE(pdir, pde, page | PG_PRESENT | PG_WRITE);
		invlpg(ROUND_DOWN(virt, 0x200000));
	}
}

#if CONFIG_X86_NX
/** Set a flag on a range of pages.
 * @param flag		Flag to set.
 * @param start		Start virtual address.
 * @param end		End virtual address.  */
static void page_set_flag(uint64_t flag, ptr_t start, ptr_t end) {
	pte_simple_t *ptbl;
	ptr_t i;

	assert(start >= KERNEL_VIRT_BASE);
	assert((start % PAGE_SIZE) == 0);
	assert((end % PAGE_SIZE) == 0);

	for(i = start; i < end; i += PAGE_SIZE) {
		page_large_to_ptbl(i);

		ptbl = (pte_simple_t *)page_map_get_ptbl(&kernel_page_map, i, false, 0);
		if(ptbl == NULL) {
			fatal("Could not get kernel page table");
		}

		ptbl[(i % 0x200000) / PAGE_SIZE] |= flag;
		invlpg(i);
	}
}
#endif

/** Set a flag on a range of pages.
 * @param flag		Flag to set.
 * @param start		Start virtual address.
 * @param end		End virtual address.  */
static void page_clear_flag(uint64_t flag, ptr_t start, ptr_t end) {
	pte_simple_t *ptbl;
	ptr_t i;

	assert(start >= KERNEL_VIRT_BASE);
	assert((start % PAGE_SIZE) == 0);
	assert((end % PAGE_SIZE) == 0);

	for(i = start; i < end; i += PAGE_SIZE) {
		page_large_to_ptbl(i);

		ptbl = (pte_simple_t *)page_map_get_ptbl(&kernel_page_map, i, false, 0);
		if(ptbl == NULL) {
			fatal("Could not get kernel page table");
		}

		ptbl[(i % 0x200000) / PAGE_SIZE] &= ~flag;
		invlpg(i);
	}
}

/** Set up the kernel page map. */
void page_init(void) {
	mutex_init(&kernel_page_map.lock, "kernel_page_map_lock");
	kernel_page_map.pml4 = KA2PA(__boot_pml4);
	kernel_page_map.user = false;
	kernel_page_map.first = KERNEL_HEAP_BASE;
	kernel_page_map.last = (ptr_t)-PAGE_SIZE;

	kprintf(LOG_DEBUG, "page: initialized kernel page map (pml4: 0x%" PRIpp ")\n", kernel_page_map.pml4);
#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		kprintf(LOG_NORMAL, "page: CPU supports NX/XD, enabling...\n");
		wrmsr(X86_MSR_IA32_EFER, rdmsr(X86_MSR_IA32_EFER) | X86_EFER_NXE);
	}
#endif
}

/** Mark kernel sections as read-only/no-execute and unmap identity mapping. */
void page_late_init(void) {
	/* Mark .text and .rodata as read-only. OK to round down - __text_start
	 * is only non-aligned because of the SIZEOF_HEADERS in the linker
	 * script. */
	page_clear_flag(PG_WRITE, ROUND_DOWN((ptr_t)__text_start, PAGE_SIZE), (ptr_t)__text_end);
	page_clear_flag(PG_WRITE, (ptr_t)__rodata_start, (ptr_t)__rodata_end);
	kprintf(LOG_DEBUG, "page: marked sections (.text .rodata) as read-only\n");

#if CONFIG_X86_NX
	/* Mark sections of the kernel no-execute if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		/* Assumes certain layout in linker script: .rodata, .data and
		 * then .bss. */
		page_set_flag(PG_NOEXEC, (ptr_t)__rodata_start, (ptr_t)__bss_end);
		kprintf(LOG_DEBUG, "page: marked sections (.rodata .data .bss) as no-execute\n");
	}
#endif

	/* Clear identity mapping. */
	SIMPLE_CLEAR_PTE(__boot_pml4, 0);
	memory_barrier();

	/* Force a complete TLB wipe - the global flag is set on pages on the
	 * identity mapping because we use the kernel PDP for it. */
	write_cr4(read_cr4() & ~X86_CR4_PGE);
	write_cr4(read_cr4() | X86_CR4_PGE);
}
