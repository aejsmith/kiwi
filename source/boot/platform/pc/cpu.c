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
 * @brief		PC CPU detection code.
 */

#include <boot/console.h>
#include <boot/cpu.h>

#include <lib/string.h>

#include <assert.h>

#include "acpi.h"
#include "mps.h"

/** Checksum a memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		True if checksum is correct, false if not. */
static inline bool checksum_range(ptr_t start, size_t size) {
	uint8_t *range = (uint8_t *)start;
	uint8_t checksum = 0;
	size_t i;

	for(i = 0; i < size; i++) {
		checksum += range[i];
	}
	return (checksum == 0);
}

/** Search for the MP Floating Pointer in a given range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		Pointer to FP if found, NULL if not. */
static mp_floating_pointer_t *mps_find_floating_pointer(ptr_t start, size_t size) {
	mp_floating_pointer_t *fp;
	size_t i;

	assert(!(start % 16));
	assert(!(size % 16));

	/* Search through the range on 16-byte boundaries. */
	for(i = 0; i < size; i += 16) {
		fp = (mp_floating_pointer_t *)(start + i);

		/* Check if the signature and checksum are correct. */
		if(strncmp(fp->signature, "_MP_", 4) != 0) {
			continue;
		} else if(!checksum_range((ptr_t)fp, (fp->length * 16))) {
			continue;
		}

		dprintf("cpu: found MPFP at %p (revision: %" PRIu8 ")\n",
		        start + i, fp->spec_rev);
		return fp;
	}

	return NULL;
}

/** Detect secondary CPUs using MP specification tables.
 * @return		Whether detection succeeded. */
static bool cpu_detect_mps(void) {
	mp_floating_pointer_t *fp;
	mp_config_table_t *cfg;
	mp_cpu_t *cpu;
	ptr_t entry;
	size_t i;

	/* Search for the MPFP structure. */
	if(!(fp = mps_find_floating_pointer(*(uint16_t *)0x40e << 4, 0x400))) {
		if(!(fp = mps_find_floating_pointer(0xE0000, 0x20000))) {
			return false;
		}
	}

	/* Check whether a MP Configuration Table was provided. */
	if(fp->phys_addr_ptr == 0) {
		dprintf("cpu: no config table provided by MPFP\n");
		return false;
	}

	/* Check that it is valid. */
	cfg = (mp_config_table_t *)fp->phys_addr_ptr;
	if(strncmp(cfg->signature, "PCMP", 4) != 0) {
		return false;
	} else if(!checksum_range((ptr_t)cfg, cfg->length)) {
		return false;
	}

	dprintf("cpu: MP config table %p revision %" PRIu8 " (%.6s %.12s)\n",
		fp->phys_addr_ptr, cfg->spec_rev, cfg->oemid, cfg->productid);

	/* Handle each entry following the table. */
	entry = (ptr_t)&cfg[1];
	for(i = 0; i < cfg->entry_count; i++) {
		switch(*(uint8_t *)entry) {
		case MP_CONFIG_CPU:
			cpu = (mp_cpu_t *)entry;
			entry += 20;

			/* Ignore disabled CPUs. */
			if(!(cpu->cpu_flags & 1)) {
				break;
			} else if(cpu->cpu_flags & 2) {
				/* This is the BSP, do a sanity check. */
				if(cpu->lapic_id != cpu_current_id()) {
					boot_error("BSP entry does not match current CPU ID");
				}
				break;
			}

			kargs_cpu_add(cpu->lapic_id);
			break;
		}
	}

	return true;
}

/** Look for the ACPI RSDP in a specific memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		Pointer to RSDP if found, NULL if not. */
static acpi_rsdp_t *acpi_find_rsdp(ptr_t start, size_t size) {
	acpi_rsdp_t *rsdp;
	size_t i;

	assert(!(start % 16));
	assert(!(size % 16));

	/* Search through the range on 16-byte boundaries. */
	for(i = 0; i < size; i += 16) {
		rsdp = (acpi_rsdp_t *)(start + i);

		/* Check if the signature and checksum are correct. */
		if(strncmp((char *)rsdp->signature, ACPI_RSDP_SIGNATURE, 8) != 0) {
			continue;
		} else if(!checksum_range((ptr_t)rsdp, 20)) {
			continue;
		}

		/* If the revision is 2 or higher, then checksum the extended
		 * fields as well. */
		if(rsdp->revision >= 2) {
			if(!checksum_range((ptr_t)rsdp, rsdp->length)) {
				continue;
			}
		}

		dprintf("cpu: found ACPI RSDP at %p (revision: %" PRIu8 ")\n",
		        start + i, rsdp->revision);
		return rsdp;
	}

	return NULL;
}

/** Search the XSDT for a table.
 * @param addr		Address of XSDT.
 * @param table		Signature of table to find.
 * @return		Pointer to table if found, NULL if not. */
static void *acpi_search_xsdt(ptr_t addr, const char *signature) {
	acpi_xsdt_t *xsdt = (acpi_xsdt_t *)addr;
	acpi_header_t *table;
	size_t count, i;

	/* Check signature and checksum. */
	if(strncmp((char *)xsdt->header.signature, ACPI_XSDT_SIGNATURE, 4) != 0) {
		dprintf("cpu: XSDT signature does not match expected signature\n");
		return NULL;
	} else if(!checksum_range(addr, xsdt->header.length)) {
		dprintf("cpu: XSDT checksum is incorrect\n");
		return NULL;
	}

	/* Load each table. */
	count = (xsdt->header.length - sizeof(xsdt->header)) / sizeof(xsdt->entry[0]);
	for(i = 0; i < count; i++) {
		table = (acpi_header_t *)((ptr_t)xsdt->entry[i]);
		if(strncmp((char *)table->signature, signature, 4) != 0) {
			continue;
		} else if(!checksum_range(xsdt->entry[i], table->length)) {
			continue;
		}

		return table;
	}

	return NULL;
}

/** Search the RSDT for a table.
 * @param addr		Address of RSDT.
 * @param table		Signature of table to find.
 * @return		Pointer to table if found, NULL if not. */
static void *acpi_search_rsdt(ptr_t addr, const char *signature) {
	acpi_rsdt_t *rsdt = (acpi_rsdt_t *)addr;
	acpi_header_t *table;
	size_t count, i;

	/* Check signature and checksum. */
	if(strncmp((char *)rsdt->header.signature, ACPI_RSDT_SIGNATURE, 4) != 0) {
		dprintf("cpu: RSDT signature does not match expected signature\n");
		return NULL;
	} else if(!checksum_range(addr, rsdt->header.length)) {
		dprintf("cpu: RSDT checksum is incorrect\n");
		return NULL;
	}

	/* Load each table. */
	count = (rsdt->header.length - sizeof(rsdt->header)) / sizeof(rsdt->entry[0]);
	for(i = 0; i < count; i++) {
		table = (acpi_header_t *)((ptr_t)rsdt->entry[i]);
		if(strncmp((char *)table->signature, signature, 4) != 0) {
			continue;
		} else if(!checksum_range(rsdt->entry[i], table->length)) {
			continue;
		}

		return table;
	}

	return NULL;
}

/** Detect secondary CPUs using ACPI.
 * @return		Whether detection succeeded. */
static bool cpu_detect_acpi(void) {
	acpi_madt_lapic_t *lapic;
	acpi_madt_t *madt = NULL;
	acpi_rsdp_t *rsdp;
	size_t i;

	/* Search for the RSDP. */
	if(!(rsdp = acpi_find_rsdp(*(uint16_t *)0x40e << 4, 0x400))) {
		if(!(rsdp = acpi_find_rsdp(0xE0000, 0x20000))) {
			return false;
		}
	}

	/* Look for the MADT using the XSDT if possible, fall back on RSDT. */
	if(rsdp->revision >= 2 && rsdp->xsdt_address != 0) {
		madt = acpi_search_xsdt(rsdp->xsdt_address, ACPI_MADT_SIGNATURE);
	}
	if(!madt) {
		madt = acpi_search_rsdt(rsdp->rsdt_address, ACPI_MADT_SIGNATURE);
	}
	if(!madt) {
		return false;
	}

	/* Add all local APICs in the table. */
	for(i = 0; i < (madt->header.length - sizeof(acpi_madt_t)); i += lapic->length) {
		lapic = (acpi_madt_lapic_t *)(madt->apic_structures + i);
		if(lapic->type != ACPI_MADT_LAPIC) {
			continue;
		} else if(!(lapic->flags & (1<<0))) {
			/* Ignore disabled processors. */
			continue;
		} else if(lapic->lapic_id == cpu_current_id()) {
			continue;
		}

		kargs_cpu_add(lapic->lapic_id);
	}

	return true;
}

/** Detect all secondary CPUs in the system. */
void cpu_detect(void) {
	/* Use ACPI if available, and fall back on MP specification tables. */
	if(!cpu_detect_acpi()) {
		cpu_detect_mps();
	}
}
