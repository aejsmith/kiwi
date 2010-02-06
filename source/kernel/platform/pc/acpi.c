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
 * @brief		PC ACPI functions.
 */

#include <arch/memmap.h>

#include <console/kprintf.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <platform/acpi.h>

#include <assert.h>
#include <fatal.h>

/** Whether ACPI is supported. */
bool acpi_supported = false;

/** Array of pointers to copies of ACPI tables. */
static acpi_header_t **acpi_tables = NULL;
static size_t acpi_table_count = 0;

/** Checksum a memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		True if checksum is correct, false if not. */
static inline bool acpi_checksum(ptr_t range, size_t size) {
	uint8_t checksum = 0;
	size_t i;

	for(i = 0; i < size; i++) {
		checksum += *(uint8_t *)(range + i);
	}

	return (checksum == 0);
}

/** Map a table, copy it and add it to the table array.
 * @param addr		Address of table. */
static void __init_text acpi_table_copy(phys_ptr_t addr) {
	acpi_header_t *source;

	/* Map the table on the heap. */
	source = page_phys_map(addr, PAGE_SIZE * 2, MM_FATAL);

	/* Check the checksum of the table. */
	if(!acpi_checksum((ptr_t)source, source->length)) {
		page_phys_unmap(source, PAGE_SIZE * 2, true);
		return;
	}

	kprintf(LOG_DEBUG, "acpi: table 0x%" PRIpp "(%.4s) revision %" PRIu8 " (%.6s %.8s %" PRIu32 ")\n",
		addr, source->signature, source->revision, source->oem_id,
		source->oem_table_id, source->oem_revision);

	/* Reallocate the table array. */
	acpi_tables = krealloc(acpi_tables, sizeof(acpi_header_t *) * ++acpi_table_count, MM_FATAL);

	/* Allocate a table structure and copy the table. */
	acpi_tables[acpi_table_count - 1] = kmalloc(source->length, MM_FATAL);
	memcpy(acpi_tables[acpi_table_count - 1], source, source->length);

	page_phys_unmap(source, PAGE_SIZE * 2, true);
}

/** Look for the ACPI RSDP in a specific memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		Pointer to RSDP if found, NULL if not. */
static inline acpi_rsdp_t *acpi_rsdp_find(phys_ptr_t start, size_t size) {
	acpi_rsdp_t *rsdp;
	size_t i;

	assert(!(start % 16));
	assert(!(size % 16));

	/* Search through the range on 16-byte boundaries. */
	for(i = 0; i < size; i += 16) {
		rsdp = (acpi_rsdp_t *)((ptr_t)start + i);

		/* Check if the signature and checksum are correct. */
		if(strncmp((char *)rsdp->signature, ACPI_RSDP_SIGNATURE, 8) != 0) {
			continue;
		} else if(!acpi_checksum((ptr_t)rsdp, 20)) {
			continue;
		}

		/* If the revision is 2 or higher, then checksum the extended
		 * fields as well. */
		if(rsdp->revision >= 2) {
			if(!acpi_checksum((ptr_t)rsdp, rsdp->length)) {
				continue;
			}
		}

		kprintf(LOG_DEBUG, "acpi: found RSDP at 0x%" PRIpp " (revision: %" PRIu8 ")\n", start + i, rsdp->revision);
		return rsdp;
	}

	return NULL;
}

/** Parse the XSDT and create a copy of all its tables.
 * @param addr		Address of XSDT.
 * @return		True if succeeded, false if not. */
static inline bool acpi_parse_xsdt(uint32_t addr) {
	acpi_xsdt_t *source;
	size_t i, count;

	source = page_phys_map(addr, PAGE_SIZE, MM_FATAL);

	/* Check signature and checksum. */
	if(strncmp((char *)source->header.signature, ACPI_XSDT_SIGNATURE, 4) != 0) {
		kprintf(LOG_DEBUG, "acpi: XSDT signature does not match expected signature\n");
		page_phys_unmap(source, PAGE_SIZE, true);
		return false;
	} else if(!acpi_checksum((ptr_t)source, source->header.length)) {
		kprintf(LOG_DEBUG, "acpi: XSDT checksum is incorrect\n");
		page_phys_unmap(source, PAGE_SIZE, true);
		return false;
	}

	/* Load each table. */
	count = (source->header.length - sizeof(source->header)) / sizeof(source->entry[0]);
	for(i = 0; i < count; i++) {
		acpi_table_copy((phys_ptr_t)source->entry[i]);
	}

	page_phys_unmap(source, PAGE_SIZE, true);
	return true;
}

/** Parse the RSDT and create a copy of all its tables.
 * @param addr		Address of RSDT.
 * @return		True if succeeded, false if not. */
static inline bool acpi_parse_rsdt(uint32_t addr) {
	acpi_rsdt_t *source;
	size_t i, count;

	source = page_phys_map(addr, PAGE_SIZE, MM_FATAL);

	/* Check signature and checksum. */
	if(strncmp((char *)source->header.signature, ACPI_RSDT_SIGNATURE, 4) != 0) {
		kprintf(LOG_DEBUG, "acpi: RSDT signature does not match expected signature\n");
		page_phys_unmap(source, PAGE_SIZE, true);
		return false;
	} else if(!acpi_checksum((ptr_t)source, source->header.length)) {
		kprintf(LOG_DEBUG, "acpi: RSDT checksum is incorrect\n");
		page_phys_unmap(source, PAGE_SIZE, true);
		return false;
	}

	/* Load each table. */
	count = (source->header.length - sizeof(source->header)) / sizeof(source->entry[0]);
	for(i = 0; i < count; i++) {
		acpi_table_copy((phys_ptr_t)source->entry[i]);
	}

	page_phys_unmap(source, PAGE_SIZE, true);
	return true;
}

/** Find an ACPI table.
 *
 * Finds an ACPI table with the given signature and returns a pointer to it.
 *
 * @param signature	Signature of table to find.
 *
 * @return		Pointer to table if found, false if not.
 */
acpi_header_t *acpi_table_find(const char *signature) {
	size_t i;

	for(i = 0; i < acpi_table_count; i++) {
		if(strncmp((char *)acpi_tables[i]->signature, signature, 4) != 0) {
			continue;
		}

		return acpi_tables[i];
	}

	return NULL;
}

/** Detect ACPI presence and find needed tables. */
void __init_text acpi_init(void) {
	acpi_rsdp_t *rsdp;
	ptr_t ebda;

	/* Get the base address of the Extended BIOS Data Area (EBDA). */
	ebda = (ptr_t)((*(uint16_t *)0x40e) << 4);

	kprintf(LOG_DEBUG, "acpi: searching for RSDP (ebda: %p)...\n", ebda);
	if(!(rsdp = acpi_rsdp_find(ebda, 0x400)) && !(rsdp = acpi_rsdp_find(0xE0000, 0x20000))) {
		kprintf(LOG_DEBUG, "acpi: cannot find RSDP, not using ACPI\n");
		return;
	}

	/* Create a copy of all the tables using the XSDT where possible. */
	if(rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
		if(!acpi_parse_xsdt(rsdp->xsdt_address)) {
			if(rsdp->rsdt_address != 0 && !acpi_parse_rsdt(rsdp->rsdt_address)) {
				return;
			}
		}
	} else if(rsdp->rsdt_address != 0) {
		if(!acpi_parse_rsdt(rsdp->rsdt_address)) {
			return;
		}
	} else {
		kprintf(LOG_DEBUG, "acpi: no XSDT/RSDT address provided, not using ACPI\n");
		return;
	}

	acpi_supported = true;
}
