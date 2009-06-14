/* Kiwi IA32 paging functions
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
#include <arch/memmap.h>
#include <arch/x86/features.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/kheap.h>
#include <mm/page.h>
#include <mm/pmm.h>

#include <assert.h>
#include <fatal.h>

/** Kernel paging structures. */
extern pte_t __kernel_pdir[];
extern uint64_t __boot_pdp[];

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

/** Macro to get address of a kernel page table. */
#define KERNEL_PTBL_ADDR(pde)		((pte_t *)(KERNEL_PTBL_BASE + ((ptr_t)(pde) * PAGE_SIZE)))

/** Kernel page map. */
page_map_t kernel_page_map;

/** Get the page table containing an address.
 * @param map		Page map to get from.
 * @param virt		Address to get page table for.
 * @param alloc		Whether to allocate structures if not found.
 * @param mmflag	Allocation flags.
 * @return		Virtual address of page table. */
static pte_t *page_map_get_ptbl(page_map_t *map, ptr_t virt, bool alloc, int mmflag) {
	phys_ptr_t page;
	pte_t *mapping;
	int pdpe, pde;

	assert(!(mmflag & PM_ZERO));

	/* Kernel mappings require special handling. */
	if(!map->user) {
		assert(virt >= 0xC0000000);

		/* Get the kernel page directory entry. */
		pde = (virt % 0x40000000) / 0x200000;
		if(!__kernel_pdir[pde].present) {
			/* Allocate a new page table if required. */
			if(!alloc || !(page = pmm_alloc(1, mmflag))) {
				return NULL;
			}

			/* Map it into the page directory. */
			__kernel_pdir[pde].address = page >> PAGE_WIDTH;
			__kernel_pdir[pde].writable = 1;
			__kernel_pdir[pde].user = 0;
			__kernel_pdir[pde].present = 1;

			/* Now clear the page table. */
			memset(KERNEL_PTBL_ADDR(pde), 0, PAGE_SIZE);
		}

		assert(!__kernel_pdir[pde].large);
		return KERNEL_PTBL_ADDR(pde);
	}

	/* Map PDP into the virtual address space. */
	mapping = page_phys_map(map->pdp, PAGE_SIZE, mmflag);
	if(mapping == NULL) {
		return NULL;
	}

	/* Get the page directory number. A page directory covers 1GB. */
	pdpe = virt / 0x40000000;
	if(!mapping[pdpe].present) {
		/* Allocate a new page directory if required. */
		if(!alloc || !(page = pmm_alloc(1, mmflag | PM_ZERO))) {
			page_phys_unmap(mapping, PAGE_SIZE);
			return NULL;
		}

		/* Map it into the PDP. */
		mapping[pdpe].address = page >> PAGE_WIDTH;
		mapping[pdpe].present = 1;

		/* Newer Intel CPUs seem to cache PDP entries and INVLPG does
		 * fuck all, completely flush the TLB if we're using this
		 * page map. */
		if((sysreg_cr3_read() & 0xFFFFF000) == map->pdp) {
			sysreg_cr3_write(sysreg_cr3_read());
		}
	} else {
		page = mapping[pdpe].address << PAGE_WIDTH;
	}

	/* Unmap PDP and map page directory. Fiddle with the mappings directly
	 * so we don't have to go through freeing and reallocating a heap
	 * range. Insert shouldn't fail because we'll already have a page
	 * table there. */
	page_map_remove(&kernel_page_map, (ptr_t)mapping, NULL);
	page_map_insert(&kernel_page_map, (ptr_t)mapping, page, PAGE_MAP_READ | PAGE_MAP_WRITE, mmflag);

	/* Get the page table number. A page table covers 2MB. */
	pde = (virt % 0x40000000) / 0x200000;
	if(!mapping[pde].present) {
		/* Allocate a new page table if required. */
		if(!alloc || !(page = pmm_alloc(1, mmflag | PM_ZERO))) {
			page_phys_unmap(mapping, PAGE_SIZE);
			return NULL;
		}

		/* Map it into the page directory. */
		mapping[pde].address = page >> PAGE_WIDTH;
		mapping[pde].writable = 1;
		mapping[pde].user = map->user;
		mapping[pde].present = 1;
	} else {
		assert(!mapping[pde].large);
		page = mapping[pde].address << PAGE_WIDTH;
	}

	/* Unmap page directory and map page table. */
	page_map_remove(&kernel_page_map, (ptr_t)mapping, NULL);
	page_map_insert(&kernel_page_map, (ptr_t)mapping, page, PAGE_MAP_READ | PAGE_MAP_WRITE, mmflag);
	return mapping;
}

/** Unmap the mapping made for a page table.
 * @param map		Page map that table was from.
 * @param ptbl		Page table to unmap. */
static void page_map_release_ptbl(page_map_t *map, pte_t *ptbl) {
	if(map->user) {
		page_phys_unmap(ptbl, PAGE_SIZE);
	}
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

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));
	assert(!(phys % PAGE_SIZE));

	/* Check that we can map here. */
	if(virt < map->first || virt > map->last) {
		fatal("Map on 0x%p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, true, mmflag);
	if(ptbl == NULL) {
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

		page_map_release_ptbl(map, ptbl);
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

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));

	/* Check that we can map here. */
	if(virt < map->first || virt > map->last) {
		fatal("Unmap on 0x%p outside allowed area", map);
	}

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, false, 0);
	if(ptbl == NULL) {
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
		ret = true;
	}

	page_map_release_ptbl(map, ptbl);
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

	assert(mutex_held(&map->lock));
	assert(!(virt % PAGE_SIZE));
	assert(physp);

	/* Find the page table for the entry. */
	ptbl = page_map_get_ptbl(map, virt, false, 0);
	if(ptbl != NULL) {
		pte = (virt % 0x200000) / PAGE_SIZE;
		if(ptbl[pte].present) {
			*physp = ptbl[pte].address << PAGE_WIDTH;
			ret = true;
		}

		page_map_release_ptbl(map, ptbl);
	}

	return ret;
}

/** Lock a page map.
 *
 * Locks a page map's lock.
 *
 * @param map		Page map to lock.
 * @param flags		Synchronization flags (see sync/flags.h).
 *
 * @return		Same as return value from mutex_lock().
 */
int page_map_lock(page_map_t *map, int flags) {
	return mutex_lock(&map->lock, flags);
}

/** Unlock a page map.
 *
 * Unlocks the specified page map.
 *
 * @param map		Page map to unlock.
 */
void page_map_unlock(page_map_t *map) {
	mutex_unlock(&map->lock);
}

/** Check whether a page map is locked.
 *
 * Checks whether a page map's lock is currently held.
 *
 * @param map		Page map to check.
 *
 * @return		Whether the lock is held.
 */
bool page_map_locked(page_map_t *map) {
	return mutex_held(&map->lock);
}

/** Switch to a different page map.
 *
 * Switches to a different page map.
 *
 * @param map		Page map to switch to.
 */
void page_map_switch(page_map_t *map) {
	sysreg_cr3_write(map->pdp);
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
	pte_t *pdp;

	mutex_init(&map->lock, "page_map_lock");
	map->pdp = pmm_xalloc(1, 0, 0, 0, 0, (phys_ptr_t)0x100000000, MM_SLEEP | PM_ZERO);
	map->user = true;
	map->first = ASPACE_BASE;
	map->last = (ASPACE_BASE + ASPACE_SIZE) - PAGE_SIZE;

	/* Get the kernel mappings into the new PDP. */
	pdp = page_phys_map(map->pdp, PAGE_SIZE, MM_SLEEP);
	pdp[3].present = 1;
	pdp[3].address = (KA2PA(__kernel_pdir)) >> PAGE_WIDTH;
	page_phys_unmap(pdp, PAGE_SIZE);

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
	pmm_free(map->pdp, 1);
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
	phys_ptr_t base, end;
	void *ret;

	if(size == 0) {
		return NULL;
	}

	/* Work out the page that the address starts on and the actual
	 * size of the mapping we need to make. */
	base = ROUND_DOWN(addr, PAGE_SIZE);
	end = ROUND_UP(addr + size, PAGE_SIZE);

	ret = kheap_map_range(base, end - base, mmflag);
	if(ret == NULL) {
		return NULL;
	}

	return (void *)((ptr_t)ret + (ptr_t)(addr - base));
}

/** Unmap physical memory.
 *
 * Unmaps a range of physical memory previously mapped with page_phys_map().
 *
 * @param addr		Virtual address returned from page_phys_map().
 * @param size		Size of original mapping.
 */
void page_phys_unmap(void *addr, size_t size) {
	ptr_t base, end;

	/* Work out the base of the allocation and its real original size. */
	base = ROUND_DOWN((ptr_t)addr, PAGE_SIZE);
	end = ROUND_UP((ptr_t)addr + size, PAGE_SIZE);

	kheap_unmap_range((void *)base, end - base);
}

/*
 * Paging initialization functions.
 */

/** Invalidate a TLB entry.
 * @param addr		Address to invalidate. */
static inline void invlpg(ptr_t addr) {
	__asm__ volatile("invlpg (%0)" :: "r"(addr));
}

/** Convert a large page to a page table if necessary.
 * @param virt		Virtual address to check. */
static void page_large_to_ptbl(ptr_t virt) {
	int pde = (virt % 0x40000000) / 0x200000, i;
	phys_ptr_t page;
	pte_t *ptbl;

	if(__kernel_pdir[pde].large) {
		page = pmm_alloc(1, MM_FATAL);
		ptbl = page_phys_map(page, PAGE_SIZE, MM_FATAL);
		memset(ptbl, 0, PAGE_SIZE);

		/* Set pages and copy all flags from the PDE. */
		for(i = 0; i < 512; i++) {
			ptbl[i].present = 1;
			ptbl[i].writable = __kernel_pdir[pde].writable;
			ptbl[i].user = 0;
			ptbl[i].global = __kernel_pdir[pde].global;
			ptbl[i].address = __kernel_pdir[pde].address + i;
			ptbl[i].noexec = __kernel_pdir[pde].noexec;
		}

		/* Replace the large page in the page directory. */
		SIMPLE_SET_PTE(__kernel_pdir, pde, page | PG_PRESENT | PG_WRITE);

		invlpg(ROUND_DOWN(virt, 0x200000));
		invlpg((ptr_t)KERNEL_PTBL_ADDR(pde));

		page_phys_unmap(ptbl, PAGE_SIZE);
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
	kernel_page_map.pdp = KA2PA(__boot_pdp);
	kernel_page_map.user = false;
	kernel_page_map.first = KERNEL_HEAP_BASE;
	kernel_page_map.last = (ptr_t)-PAGE_SIZE;

	kprintf(LOG_DEBUG, "page: initialized kernel page map (pdp: 0x%" PRIpp ")\n", kernel_page_map.pdp);
#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(CPU_HAS_XD(curr_cpu)) {
		kprintf(LOG_DEBUG, "page: CPU supports NX/XD, enabling...\n");
		sysreg_msr_write(SYSREG_MSR_EFER, sysreg_msr_read(SYSREG_MSR_EFER) | SYSREG_EFER_NXE);
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

	/* Clear identity mapping and flush it out of the TLB. */
	SIMPLE_CLEAR_PTE(__boot_pdp, 0);
	memory_barrier();
	sysreg_cr3_write(sysreg_cr3_read());
}
