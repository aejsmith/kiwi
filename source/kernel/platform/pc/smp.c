/*
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		PC secondary CPU detection.
 *
 * The functions in this file are used to detect CPUs in the system the kernel
 * is running it. ACPI is used to perform detection on systems that support
 * it, and the MP Specification tables are used as a fallback. The actual CPU
 * boot code is implemented by the architecture in use.
 */

#include <arch/lapic.h>

#include <cpu/cpu.h>
#include <cpu/smp.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/page.h>

#include <platform/acpi.h>
#include <platform/mps.h>

#include <assert.h>
#include <console.h>
#include <fatal.h>

/** Checksum a memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		True if checksum is correct, false if not. */
static inline bool smp_mps_checksum(uint8_t *range, size_t size) {
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
static inline mp_fptr_t *smp_mps_find_fp(phys_ptr_t start, size_t size) {
	mp_fptr_t *fp;
	size_t i;

	assert(!(start % 16));
	assert(!(size % 16));

	/* Search through the range on 16-byte boundaries. */
	for(i = 0; i < size; i += 16) {
		fp = (mp_fptr_t *)((ptr_t)start + i);

		/* Check if the signature and checksum are correct. */
		if(strncmp(fp->signature, "_MP_", 4) != 0) {
			continue;
		} else if(!smp_mps_checksum((uint8_t *)fp, (fp->length * 16))) {
			continue;
		}

		kprintf(LOG_DEBUG, "cpu: found MPFP at 0x%" PRIpp " (revision: %" PRIu8 ")\n",
		        start + i, fp->spec_rev);
		return fp;
	}

	return NULL;
}

/** Detect CPUs using the MPS tables.
 * @return		True if succeeded, false if not. */
static inline bool smp_detect_mps(void) {
	ptr_t ebda, entry;
	mp_config_t *cfg;
	mp_fptr_t *fp;
	mp_cpu_t *cpu;
	size_t i;

	/* Get the base address of the Extended BIOS Data Area (EBDA). */
	ebda = (ptr_t)((*(uint16_t *)0x40e) << 4);

	/* Search for the MPFP structure. */
	if(!(fp = smp_mps_find_fp(ebda, 0x400)) && !(fp = smp_mps_find_fp(0xE0000, 0x20000))) {
		return false;
	}

	/* Check whether a MP Configuration Table was provided. */
	if(fp->phys_addr_ptr == 0) {
		kprintf(LOG_DEBUG, "cpu: no config table provided by MPFP table\n");
		return false;
	}

	/* Map the config table onto the kernel heap. */
	cfg = page_phys_map(fp->phys_addr_ptr, PAGE_SIZE, MM_FATAL);

	/* Check that it is valid. */
	if(strncmp(cfg->signature, "PCMP", 4) != 0) {
		page_phys_unmap(cfg, PAGE_SIZE, true);
		return false;
	} else if(!smp_mps_checksum((uint8_t *)cfg, cfg->length)) {
		page_phys_unmap(cfg, PAGE_SIZE, true);
		return false;
	}

	kprintf(LOG_DEBUG, "cpu: config table 0x%" PRIx32 " revision %" PRIu8 " (%.6s %.12s)\n",
		fp->phys_addr_ptr, cfg->spec_rev, cfg->oemid, cfg->productid);

	/* Handle each entry following the table. */
	for(i = 0, entry = (ptr_t)&cfg[1]; i < cfg->entry_count; i++) {
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
					fatal("BSP entry does not match current CPU ID");
				}
				break;
			}

			cpu_add((cpu_id_t)cpu->lapic_id, CPU_DOWN);
			break;
		}
	}

	page_phys_unmap(cfg, PAGE_SIZE, true);
	return true;
}

/** Detect CPUs using ACPI.
 * @return		True if succeeded, false if not. */
static inline bool smp_detect_acpi(void) {
	acpi_madt_lapic_t *lapic;
	acpi_madt_t *madt;
	size_t i, length;

	madt = (acpi_madt_t *)acpi_table_find(ACPI_MADT_SIGNATURE);
	if(madt == NULL) {
		return false;
	}

	length = madt->header.length - sizeof(acpi_madt_t);
	for(i = 0; i < length; i += lapic->length) {
		lapic = (acpi_madt_lapic_t *)(madt->apic_structures + i);

		if(lapic->type != ACPI_MADT_LAPIC) {
			continue;
		} else if(!(lapic->flags & (1<<0))) {
			/* Ignore disabled processors. */
			continue;
		} else if(lapic->lapic_id == cpu_current_id()) {
			continue;
		}

		/* Add and boot the CPU. */
		cpu_add((cpu_id_t)lapic->lapic_id, CPU_DOWN);
	}
	
	return true;
}

/** Detect secondary CPUs.
 *
 * Detects all secondary CPUs in the current system, using the ACPI tables
 * where possible. Falls back on the MP tables if ACPI is unavailable.
 */
void __init_text smp_detect_cpus(void) {
	/* Do not do anything if we do not have a local APIC on the BSP. */
	if(!lapic_enabled) {
		return;
	}

	/* Use ACPI where supported. */
	if(!acpi_supported || !smp_detect_acpi()) {
		if(!smp_detect_mps()) {
			kprintf(LOG_DEBUG, "smp: neither ACPI or MPS are available for CPU detection\n");
			return;
		}
	}

	kprintf(LOG_DEBUG, "smp: detected %zu CPU(s)\n", cpu_count);
}
