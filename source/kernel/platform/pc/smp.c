/*
 * Copyright (C) 2008-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		PC SMP detection code.
 */

#include <x86/lapic.h>

#include <pc/acpi.h>
#include <pc/mps.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/phys.h>

#include <assert.h>
#include <kernel.h>
#include <smp.h>

/** Search for the MP Floating Pointer in a given range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		Pointer to FP if found, NULL if not. Must be unmapped
 *			with phys_unmap(). */
static mp_floating_pointer_t *mps_find_floating_pointer(ptr_t start, size_t size) {
	mp_floating_pointer_t *fp;
	size_t i;

	assert(!(start % 16));
	assert(!(size % 16));

	/* Search through the range on 16-byte boundaries. */
	for(i = 0; i < size; i += 16) {
		fp = phys_map(start + i, sizeof(*fp), MM_FATAL);

		/* Check if the signature and checksum are correct. */
		if(strncmp(fp->signature, "_MP_", 4) != 0) {
			phys_unmap(fp, sizeof(*fp), true);
			continue;
		} else if(!checksum_range(fp, (fp->length * 16))) {
			phys_unmap(fp, sizeof(*fp), true);
			continue;
		}

		kprintf(LOG_DEBUG, "cpu: found MPFP at %p (revision: %" PRIu8 ")\n",
		        start + i, fp->spec_rev);
		return fp;
	}

	return NULL;
}

/** Detect secondary CPUs using MP specification tables.
 * @return		Whether detection succeeded. */
static bool smp_detect_mps(void) {
	mp_floating_pointer_t *fp;
	mp_config_table_t *cfg;
	uint16_t *mapping;
	phys_ptr_t ebda;
	mp_cpu_t *cpu;
	ptr_t entry;
	size_t i;

	/* Get the base address of the Extended BIOS Data Area (EBDA). */
	mapping = phys_map(0x40e, sizeof(uint16_t), MM_FATAL);
	ebda = (*mapping) << 4;
	phys_unmap(mapping, sizeof(uint16_t), true);

	/* Search for the MPFP structure. */
	if(!(fp = mps_find_floating_pointer(ebda, 0x400))) {
		if(!(fp = mps_find_floating_pointer(0xE0000, 0x20000))) {
			return false;
		}
	}

	/* Check whether a MP Configuration Table was provided. */
	if(fp->phys_addr_ptr == 0) {
		kprintf(LOG_DEBUG, "cpu: no config table provided by MPFP\n");
		phys_unmap(fp, sizeof(*fp), true);
		return false;
	}

	cfg = phys_map(fp->phys_addr_ptr, PAGE_SIZE, MM_FATAL);
	phys_unmap(fp, sizeof(*fp), true);

	/* Check that it is valid. */
	if(strncmp(cfg->signature, "PCMP", 4) != 0) {
		phys_unmap(cfg, PAGE_SIZE, true);
		return false;
	} else if(!checksum_range(cfg, cfg->length)) {
		phys_unmap(cfg, PAGE_SIZE, true);
		return false;
	}

	kprintf(LOG_DEBUG, "cpu: MP config table revision %" PRIu8 " (%.6s %.12s)\n",
		cfg->spec_rev, cfg->oemid, cfg->productid);

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
				if(cpu->lapic_id != curr_cpu->id) {
					fatal("BSP entry does not match current CPU ID");
				}
				break;
			}

			cpu_register(cpu->lapic_id, CPU_OFFLINE);
			break;
		}
	}

	phys_unmap(cfg, PAGE_SIZE, true);
	return true;
}

/** Detect secondary CPUs using ACPI.
 * @return		Whether detection succeeded. */
static inline bool smp_detect_acpi(void) {
	acpi_madt_lapic_t *lapic;
	acpi_madt_t *madt;
	size_t i, length;

	madt = (acpi_madt_t *)acpi_table_find(ACPI_MADT_SIGNATURE);
	if(!madt) {
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
		} else if(lapic->lapic_id == curr_cpu->id) {
			continue;
		}

		cpu_register(lapic->lapic_id, CPU_OFFLINE);
	}
	
	return true;
}

/** Detect all secondary CPUs in the system. */
void platform_smp_detect(void) {
	/* If the LAPIC is disabled, we cannot use SMP. */
	if(!lapic_enabled()) {
		kprintf(LOG_NOTICE, "cpu: disabling SMP due to lack of APIC support\n");
		return;
	}

	/* Use ACPI if available, and fall back on MP specification tables. */
	if(!acpi_supported || !smp_detect_acpi()) {
		smp_detect_mps();
	}
}
