/* Kiwi x86 physical memory manager functions
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
 * @brief		x86 physical memory manager functions.
 */

#include <arch/features.h>
#include <arch/mem.h>
#include <arch/multiboot.h>
#include <arch/page.h>

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <lib/utility.h>

#include <mm/aspace.h>
#include <mm/pmm.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_PMM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

extern void pmm_arch_init(multiboot_info_t *info);

extern char __init_start[];
extern char __init_end[];
extern char __end[];

/** Set up the memory map.
 *
 * Uses the memory map provided by the bootloader to set up the physical
 * memory manager with free regions and marks certain regions as reserved or
 * reclaimable.
 *
 * @todo		Check that addresses are within the physical address
 *			size supported by the processor.
 *
 * @param info		Pointer to Multiboot information structure.
 */
void pmm_arch_init(multiboot_info_t *info) {
	multiboot_memmap_t *map = (multiboot_memmap_t *)((ptr_t)info->mmap_addr);
	multiboot_module_t *mods = (multiboot_module_t *)((ptr_t)info->mods_addr);
	phys_ptr_t start, end;
	size_t i;

	assert(((ptr_t)__init_start % PAGE_SIZE) == 0);
	assert(((ptr_t)__init_end % PAGE_SIZE) == 0);
	assert(((ptr_t)__end % PAGE_SIZE) == 0);

	dprintf("pmm: adding E820 memory map entries...\n");

	/* Go through the Multiboot memory map and add everything in it. We
	 * can safely access the memory map because of the temporary
	 * identity mapping (unless the bootloader decides to stick the
	 * memory map ridiculously high up in memory. Smile and wave, boys,
	 * smile and wave...). */
	while(map < (multiboot_memmap_t *)((ptr_t)info->mmap_addr + info->mmap_length)) {
		if(map->length == 0) {
			/* Ignore zero-length entries. */
			goto cont;
		}

		dprintf(" 0x%016" PRIx64 " - 0x%016" PRIx64 " (%" PRIu32 ")\n", map->base_addr, map->base_addr + map->length, map->type);

		/* We only want to add free and reclaimable regions. FIXME */
		if(map->type != E820_TYPE_FREE/* && map->type != E820_TYPE_ACPI_RECLAIM*/) {
			goto cont;
		}

		/* The E820 memory map can contain regions that aren't
		 * page-aligned. This presents a problem for us - we want to
		 * create a list of regions for the page allocator that are all
		 * page-aligned. Therefore, we round start up and end down, to
		 * ensure that the region doesn't get resized to cover memory
		 * we shouldn't access. If this results in a zero-length entry,
		 * then we ignore it. */
		start = ROUND_UP(map->base_addr, PAGE_SIZE);
		end = ROUND_DOWN(map->base_addr + map->length, PAGE_SIZE);
		// FIXME temporary
		if(start < KA2PA((ptr_t)__end)) {
			start = KA2PA((ptr_t)__end);
		}

		/* What we did above may have made the region too small, warn
		 * and ignore it if this is the case. */
		if(end <= start) {
			kprintf(LOG_NORMAL, "pmm: broken memory map entry: [0x%" PRIx64 ",0x%" PRIx64 ") (%" PRIu32 ")\n",
				map->base_addr, map->base_addr + map->length, map->type);
			goto cont;
		}

		/* Add the range and mark as reclaimable if necessary. */
		pmm_add(start, end);
		if(map->type == E820_TYPE_ACPI_RECLAIM) {
			pmm_mark_reclaimable(start, end);
		}
	cont:
		map = (multiboot_memmap_t *)(((ptr_t)map) + map->size + 4);
	}

	/* Mark the kernel as reserved and initialization code/data as
	 * reclaimable. */
	pmm_mark_reserved(KERNEL_PHYS_BASE, KA2PA((ptr_t)__init_start));
	pmm_mark_reclaimable(KA2PA((ptr_t)__init_start), KA2PA((ptr_t)__init_end));
	pmm_mark_reserved(KA2PA((ptr_t)__init_end), KA2PA((ptr_t)__end));

	/* Mark all the Multiboot modules as reclaimable. Start addresses
	 * should be page-aligned because we specify we want that to be the
	 * case in the Multiboot header. */
	for(i = 0; i < info->mods_count; i++) {
		assert(!(mods[i].mod_start % PAGE_SIZE));

		pmm_mark_reclaimable(mods[i].mod_start, ROUND_UP(mods[i].mod_end, PAGE_SIZE));
	}
}
