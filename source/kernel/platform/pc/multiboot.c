/* Kiwi Multiboot specification functions
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
 * @brief		Multiboot specification functions.
 */

#include <arch/memmap.h>

#include <console/kprintf.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/pmm.h>

#include <platform/multiboot.h>

#include <assert.h>
#include <bootmod.h>
#include <fatal.h>

#if CONFIG_PMM_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)	
#endif

/** Check for a flag in a Multiboot information structure. */
#define CHECK_MB_FLAG(i, f)	\
	if(((i)->flags & (f)) == 0) { \
		fatal("Required flag not set: " #f); \
	}

extern char __init_start[], __init_end[], __end[];

/** Pointer to Multiboot information structure. */
static multiboot_info_t *mb_info;

/** Multiboot module information. */
static bootmod_t *mb_modules;
static size_t mb_module_count;

/** Populate the PMM with memory regions.
 *
 * Uses the memory map provided by the bootloader to set up the physical
 * memory manager with free regions and marks certain regions as reserved or
 * reclaimable.
 *
 * @todo		Check that addresses are within the physical address
 *			size supported by the processor.
 */
void pmm_populate(void) {
	multiboot_memmap_t *map = (multiboot_memmap_t *)((ptr_t)mb_info->mmap_addr);
	multiboot_module_t *mods = (multiboot_module_t *)((ptr_t)mb_info->mods_addr);
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
	while(map < (multiboot_memmap_t *)((ptr_t)mb_info->mmap_addr + mb_info->mmap_length)) {
		if(map->length == 0) {
			/* Ignore zero-length entries. */
			goto cont;
		}

		dprintf(" 0x%016" PRIx64 " - 0x%016" PRIx64 " (%" PRIu32 ")\n",
		        map->base_addr, map->base_addr + map->length, map->type);

		/* We only want to add free and reclaimable regions. */
		if(map->type != E820_TYPE_FREE && map->type != E820_TYPE_ACPI_RECLAIM) {
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

		/* For now we will ignore this... */
		if(start == 0) {
			goto cont;
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
	for(i = 0; i < mb_info->mods_count; i++) {
		assert(!(mods[i].mod_start % PAGE_SIZE));

		pmm_mark_reclaimable(mods[i].mod_start, ROUND_UP(mods[i].mod_end, PAGE_SIZE));
	}
}

/** Get an array of boot modules.
 *
 * Gets an array of structures representing each module provided by the
 * bootloader to the kernel. The returned array should be freed with kfree().
 *
 * @param arrp		Where to store pointer to module array.
 *
 * @return		Number of modules in array.
 */
size_t bootmod_get(bootmod_t **arrp) {
	*arrp = mb_modules;
	return mb_module_count;
}

/** Check and store Multiboot information.
 *
 * Checks the provided Multiboot information structure and stores a pointer
 * to it.
 *
 * @param info		Multiboot information pointer.
 */
void multiboot_premm_init(multiboot_info_t *info) {
	/* Check for required Multiboot flags. */
	CHECK_MB_FLAG(info, MB_FLAG_MEMINFO);
	CHECK_MB_FLAG(info, MB_FLAG_MMAP);
	CHECK_MB_FLAG(info, MB_FLAG_CMDLINE);

	/* Store a pointer to the structure for later use. */
	mb_info = info;
}

/** Save a copy of all required Multiboot information.
 *
 * Saves a copy of all required Multiboot information such as modules and
 * kernel command line. This is done because their virtual addresses get
 * unmapped by the architecture, and their current physical location is
 * reclaimed by the PMM.
 */
void multiboot_postmm_init(void) {
	multiboot_module_t *mods = (multiboot_module_t *)((ptr_t)mb_info->mods_addr);
	char *name, *tmp;
	size_t i;

	if(mb_info->mods_count == 0) {
		return;
	}

	/* Save a copy of all modules - convert multiboot_module_t
	 * structures to bootmod_t. */
	mb_module_count = mb_info->mods_count;
	mb_modules = kcalloc(mb_module_count, sizeof(bootmod_t), MM_FATAL);
	for(i = 0; i < mb_module_count; i++) {
		/* We only want the base name, take off any path strings. */
		name = strrchr((char *)((ptr_t)mods[i].string), '/');
		if(name == NULL) {
			name = (char *)((ptr_t)mods[i].string);
		} else {
			/* Point past the /. */
			name += 1;
		}

		/* Split off arguments to the module. */
		tmp = name;
		strsep(&tmp, " ");

		/* Duplicate the name string and the module data. */
		mb_modules[i].name = kstrdup(name, MM_FATAL);
		mb_modules[i].size = mods[i].mod_end - mods[i].mod_start;
		mb_modules[i].addr = kmemdup((void *)((ptr_t)mods[i].mod_start), mb_modules[i].size, MM_FATAL);
	}
}
