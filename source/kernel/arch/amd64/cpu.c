/* Kiwi x86 CPU management
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
 * @brief		x86 CPU management.
 */

#include <arch/apic.h>
#include <arch/asm.h>
#include <arch/defs.h>
#include <arch/features.h>
#include <arch/mps.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/kheap.h>
#include <mm/page.h>

#include <platform/acpi.h>

#include <time/timer.h>

#include <assert.h>
#include <fatal.h>
#include <kdbg.h>

#if CONFIG_SMP

extern char __ap_trampoline_start[];
extern char __ap_trampoline_end[];
extern atomic_t ap_boot_wait;

extern void __kernel_ap_entry(void);

/** Stack pointer AP should use during boot. */
void *ap_stack_ptr = 0;

/** Waiting variable for cpu_boot_delay(). */
static atomic_t cpu_boot_delay_wait = 0;

/** Checksum a memory range.
 * @param start		Start of range to check.
 * @param size		Size of range to check.
 * @return		True if checksum is correct, false if not. */
static bool cpu_mps_checksum(uint8_t *range, size_t size) {
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
static mp_fptr_t *cpu_mps_find_fp(phys_ptr_t start, size_t size) {
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
		} else if(!cpu_mps_checksum((uint8_t *)fp, (fp->length * 16))) {
			continue;
		}

		kprintf(LOG_DEBUG, "cpu: found MPFP at 0x%" PRIpp " (revision: %" PRIu8 ")\n", start + i, fp->spec_rev);
		return fp;
	}

	return NULL;
}

/** CPU boot delay timer handler. */
static bool cpu_boot_delay_func(void) {
	atomic_set(&cpu_boot_delay_wait, 1);
	return false;
}

/** Delay for a number of µseconds during CPU startup.
 * @param us		Number of µseconds to wait. */
static void cpu_boot_delay(uint64_t us) {
	timer_t timer;

	atomic_set(&cpu_boot_delay_wait, 0);

	timer_init(&timer, TIMER_FUNCTION, cpu_boot_delay_func);
	intr_enable();
	timer_start(&timer, us * 1000);

	while(atomic_get(&cpu_boot_delay_wait) == 0);
	intr_disable();
}

/** Detect CPUs using ACPI.
 * @return		True if succeeded, false if not. */
static bool cpu_detect_acpi(void) {
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
		} else if(lapic->lapic_id == apic_local_id()) {
			continue;
		}

		/* Add and boot the CPU. */
		cpu_add((cpu_id_t)lapic->lapic_id, CPU_DOWN);
	}
	
	return true;
}

/** Detect CPUs using the MPS tables.
 * @return		True if succeeded, false if not. */
static bool cpu_detect_mps(void) {
	ptr_t ebda, entry;
	mp_config_t *cfg;
	mp_fptr_t *fp;
	mp_cpu_t *cpu;
	size_t i;

	/* Get the base address of the Extended BIOS Data Area (EBDA). */
	ebda = (ptr_t)(*(uint16_t *)0x40e << 4);

	/* Search for the MPFP structure. */
	if(!(fp = cpu_mps_find_fp(ebda, 0x400)) && !(fp = cpu_mps_find_fp(0xE0000, 0x20000))) {
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
		page_phys_unmap(cfg, PAGE_SIZE);
		return false;
	} else if(!cpu_mps_checksum((uint8_t *)cfg, cfg->length)) {
		page_phys_unmap(cfg, PAGE_SIZE);
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
				if(cpu->lapic_id != apic_local_id()) {
					fatal("BSP entry does not match current CPU ID");
				}
				break;
			}

			cpu_add((cpu_id_t)cpu->lapic_id, CPU_DOWN);
			break;
		}
	}

	page_phys_unmap(cfg, PAGE_SIZE);
	return true;
}

/** Boot a CPU.
 *
 * Boots a secondary CPU.
 *
 * @param id		Local APIC ID.
 */
void cpu_boot(cpu_t *cpu) {
	uint32_t delay = 0;
	uint32_t *dest;
	size_t size;
	void *stack;

	kprintf(LOG_DEBUG, "cpu: booting CPU %" PRIu32 " (0x%p)...\n", cpu->id, cpu);
	atomic_set(&ap_boot_wait, 0);

	/* Copy the trampoline code to 0x7000 and set the entry point address. */
	size = (ptr_t)__ap_trampoline_end - (ptr_t)__ap_trampoline_start;
	dest = page_phys_map(0x7000, size, MM_FATAL);
	memcpy(dest, (void *)__ap_trampoline_start, size);
	dest[1] = (uint32_t)KA2PA(__kernel_ap_entry);
	page_phys_unmap(dest, size);

	/* Allocate a new stack and set the CPU structure pointer. */
	stack = kheap_alloc(KSTACK_SIZE, MM_FATAL);
	*(ptr_t *)stack = (ptr_t)cpu;
	ap_stack_ptr = stack + KSTACK_SIZE;

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	apic_ipi(IPI_DEST_SINGLE, cpu->id, APIC_IPI_INIT, 0x00);
	cpu_boot_delay(10000);

	/* Send a SIPI. The 0x07 argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	apic_ipi(IPI_DEST_SINGLE, cpu->id, APIC_IPI_SIPI, 0x07);
	cpu_boot_delay(10000);

	/* If the CPU is up, then return. */
	if(atomic_get(&ap_boot_wait)) {
		return;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	apic_ipi(IPI_DEST_SINGLE, cpu->id, APIC_IPI_SIPI, 0x07);
	while(delay < 5000000) {
		if(atomic_get(&ap_boot_wait)) {
			return;
		}
		cpu_boot_delay(10000);
		delay += 10000;
	}

	fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
}

/** Detect secondary CPUs.
 *
 * Detects all secondary CPUs in the current system, using the ACPI tables
 * where possible. Falls back on the MP tables if ACPI is unavailable.
 */
void cpu_detect(void) {
	/* If there is no APIC, do not do anything. */
	if(!apic_supported) {
		return;
	}

	/* Use ACPI where supported. */
	if(!acpi_supported || !cpu_detect_acpi()) {
		if(!cpu_detect_mps()) {
			kprintf(LOG_DEBUG, "cpu: neither ACPI or MPS are available for CPU detection\n");
			return;
		}
	}

	kprintf(LOG_DEBUG, "cpu: detected %" PRIs " CPU(s)\n", cpu_count);
}

/** Send an IPI.
 *
 * Sends an IPI (inter-processor interrupt) to the specified processors.
 *
 * @param dest		Where to send the IPI to.
 * @param id		Destination CPU ID for IPI_DEST_SINGLE.
 * @param vector	IPI vector.
 */
void cpu_ipi(uint8_t dest, cpu_id_t id, uint8_t vector) {
	if(vector == IPI_KDBG || vector == IPI_FATAL) {
		apic_ipi(dest, id, APIC_IPI_NMI, 0);
	} else {
		apic_ipi(dest, id, APIC_IPI_FIXED, vector);
	}
}
#endif /* CONFIG_SMP */

/** Get current CPU ID.
 *
 * Gets the ID of the CPU that the function executes on.
 *
 * @return		Current CPU ID.
 */
cpu_id_t cpu_current_id(void) {
	return (cpu_id_t)apic_local_id();
}

/** Initialize an x86 CPU information structure.
 *
 * Fills in the given x86 CPU information structure with information about
 * the current CPU. Assumes CPUID is supported - should be checked in the
 * boot code.
 *
 * @param cpu		CPU information structure to fill in.
 */
void cpu_arch_init(cpu_arch_t *cpu) {
	uint32_t eax, ebx, ecx, edx;
	size_t i = 0, j = 0;
	uint32_t *ptr;
	char *str;

	/* Get the highest supported standard level. */
	cpuid(CPUID_VENDOR_ID, &cpu->features.largest_standard, &ebx, &ecx, &edx);
	if(cpu->features.largest_standard >= CPUID_FEATURE_INFO) {
		/* Get standard feature information. */
		cpuid(CPUID_FEATURE_INFO, &eax, &ebx, &cpu->features.feat_ecx, &cpu->features.feat_edx);
		cpu->family = (eax >> 8) & 0x0f;
		cpu->model = (eax >> 4) & 0x0f;
		cpu->stepping = eax & 0x0f;
	}

	/* Get the highest supported extended level. */
	cpuid(CPUID_EXT_MAX, &cpu->features.largest_extended, &ebx, &ecx, &edx);
	if(cpu->features.largest_extended & (1<<31)) {
		if(cpu->features.largest_extended >= CPUID_EXT_FEATURE) {
			/* Get extended feature information. */
			cpuid(CPUID_EXT_FEATURE, &eax, &ebx, &cpu->features.ext_ecx, &cpu->features.ext_edx);
		}

		if(cpu->features.largest_extended >= CPUID_BRAND_STRING3) {
			/* Get brand information. */
			memset(cpu->model_name, 0, sizeof(cpu->model_name));
			str = cpu->model_name;
			ptr = (uint32_t *)str;

			cpuid(CPUID_BRAND_STRING1, &ptr[0], &ptr[1], &ptr[2],  &ptr[3]);
			cpuid(CPUID_BRAND_STRING2, &ptr[4], &ptr[5], &ptr[6],  &ptr[7]);
			cpuid(CPUID_BRAND_STRING3, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

			/* Some CPUs right-justify the string... */
			while(str[i] == ' ') {
				i++;
			}
			if(i > 0) {
				while(str[i]) {
					str[j++] = str[i++];
				}
				while(j < sizeof(cpu->model_name)) {
					str[j++] = 0;
				}
			}
		}
	} else {
		cpu->features.largest_extended = 0;
	}
}

/** CPU information command for KDBG.
 *
 * Prints a list of all CPUs and information about them.
 *
 * @param argc		Argument count.
 * @param argv		Argument array.
 *
 * @return		KDBG_OK on success.
 */
int kdbg_cmd_cpus(int argc, char **argv) {
	size_t i;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_KDBG, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_KDBG, "Prints a list of all CPUs and information about them.\n");
		return KDBG_OK;
	}

	kprintf(LOG_KDBG, "ID   State    Model Name\n");
	kprintf(LOG_KDBG, "==   =====    ==========\n");

	for(i = 0; i <= cpu_id_max; i++) {
		if(cpus[i] == NULL) {
			continue;
		}

		kprintf(LOG_KDBG, "%-4" PRIu32 " ", cpus[i]->id);
		switch(cpus[i]->state) {
		case CPU_DISABLED:	kprintf(LOG_KDBG, "Disabled "); break;
		case CPU_DOWN:		kprintf(LOG_KDBG, "Down     "); break;
		case CPU_RUNNING:	kprintf(LOG_KDBG, "Running  "); break;
		default:		kprintf(LOG_KDBG, "Bad      "); break;
		}
		kprintf(LOG_KDBG, "%s\n", (cpus[i]->arch.model_name[0]) ? cpus[i]->arch.model_name : "Unknown");
	}

	return KDBG_OK;
}
