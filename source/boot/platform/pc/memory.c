/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		PC memory detection code.
 */

#include <boot/console.h>
#include <boot/error.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include "bios.h"

/** Memory map type values. */
#define E820_TYPE_FREE		1		/**< Usable memory. */
#define E820_TYPE_RESERVED	2		/**< Reserved memory. */
#define E820_TYPE_ACPI_RECLAIM	3		/**< ACPI reclaimable. */
#define E820_TYPE_ACPI_NVS	4		/**< ACPI NVS. */

/** E820 memory map entry structure. */
typedef struct e820_entry {
	uint64_t start;			/**< Start of range. */
	uint64_t length;		/**< Length of range. */
	uint32_t type;			/**< Type of range. */
} __packed e820_entry_t;

/** Detect physical memory. */
void platform_memory_detect(void) {
	e820_entry_t *mmap = (e820_entry_t *)BIOS_MEM_BASE;
	phys_ptr_t start, end;
	size_t count = 0, i;
	bios_regs_t regs;
	int type;

	bios_regs_init(&regs);

	/* Obtain a memory map using interrupt 15h, function E820h. */
	do {
		regs.eax = 0xE820;
		regs.edx = 0x534D4150;
		regs.ecx = 20;
		regs.edi = BIOS_MEM_BASE + (count * sizeof(e820_entry_t));
		bios_interrupt(0x15, &regs);

		/* If CF is set, the call was not successful. BIOSes are
		 * allowed to return a non-zero continuation value in EBX and
		 * return an error on next call to indicate that the end of the
		 * list has been reached. */
		if(regs.eflags & X86_FLAGS_CF) {
			break;
		}

		count++;
	} while(regs.ebx != 0);

	/* FIXME: Should handle BIOSen that don't support this. */
	if(count == 0) {
		boot_error("BIOS does not support E820 memory map");
	}

	/* Iterate over the obtained memory map and add the entries to the
	 * PMM. */
	for(i = 0; i < count; i++) {
		/* The E820 memory map can contain regions that aren't
		 * page-aligned. This presents a problem for us - we want to
		 * provide the kernel with a list of regions that are all
		 * page-aligned. Therefore, we must handle non-aligned regions
		 * according to the type they are. For free memory regions,
		 * we round start up and end down, to ensure that the region
		 * doesn't get resized to memory we shouldn't accessed. If
		 * this results in a zero-length entry, then we ignore it.
		 * Otherwise, we round start down, and end up, so we don't
		 * finish up with a zero-length region. This ensures that all
		 * reserved regions in the original map are included in the
		 * map provided to the kernel. */
		if(mmap[i].type == E820_TYPE_FREE || mmap[i].type == E820_TYPE_ACPI_RECLAIM) {
			start = ROUND_UP(mmap[i].start, PAGE_SIZE);
			end = ROUND_DOWN(mmap[i].start + mmap[i].length, PAGE_SIZE);
		} else {
			start = ROUND_DOWN(mmap[i].start, PAGE_SIZE);
			end = ROUND_UP(mmap[i].start + mmap[i].length, PAGE_SIZE);
		}

		/* What we did above may have made the region too small, warn
		 * and ignore it if this is the case. */
		if(end <= start) {
			dprintf("memory: broken memory map entry: [0x%" PRIx64 ",0x%" PRIx64 ") (%" PRIu32 ")\n",
				mmap[i].start, mmap[i].start + mmap[i].length, mmap[i].type);
			continue;
		}

		/* Work out the type to give the range. */
		switch(mmap[i].type) {
		case E820_TYPE_FREE:
			type = PHYS_MEMORY_FREE;
			break;
		case E820_TYPE_ACPI_RECLAIM:
			type = PHYS_MEMORY_RECLAIMABLE;
			break;
		case E820_TYPE_RESERVED:
		case E820_TYPE_ACPI_NVS:
			type = PHYS_MEMORY_RESERVED;
			break;
		default:
			continue;
		}

		/* Add the range to the physical memory manager. */
		phys_memory_add(start, end, type);
	}

	/* Ensure that the BIOS data area is marked as reserved - BIOSes don't
	 * mark it as reserved in the memory map as it can be overwritten if it
	 * is no longer needed, but it is needed in the kernel to call BIOS
	 * interrupts. */
	phys_memory_add(0, PAGE_SIZE, PHYS_MEMORY_RESERVED);

	/* Mark the memory area we use for BIOS calls as internal. */
	phys_memory_add(BIOS_MEM_BASE, BIOS_MEM_BASE + BIOS_MEM_SIZE + PAGE_SIZE,
	                PHYS_MEMORY_INTERNAL);
}
