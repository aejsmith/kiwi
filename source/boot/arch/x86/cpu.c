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
 * @brief		x86 CPU detection functions.
 */

#include <arch/x86/cpu.h>
#include <arch/x86/lapic.h>
#include <arch/boot.h>
#include <arch/io.h>
#include <arch/memory.h>

#include <boot/console.h>
#include <boot/cpu.h>
#include <boot/error.h>
#include <boot/memory.h>

#include <lib/string.h>
#include <lib/utility.h>

#include <assert.h>
#include <time.h>

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

/** Number of times to get a frequency (must be odd). */
#define FREQUENCY_ATTEMPTS	9

extern char __ap_trampoline_start[], __ap_trampoline_end[];
extern void cpu_ap_entry(void);

/** Address of the local APIC. */
static volatile uint32_t *lapic_mapping = NULL;

/** Variables used in the AP boot process. */
static void (*ap_entry_func)(void);
static atomic_t ap_boot_wait = 0;
static kernel_args_cpu_t *booting_cpu = NULL;

/** Stack pointer for the booting AP. */
ptr_t ap_stack_ptr = 0;

/** Read the Time Stamp Counter.
 * @return		Value of the TSC. */
static inline uint64_t rdtsc(void) {
	uint32_t high, low;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return ((uint64_t)high << 32) | low;
}

/** Comparison function for qsort() on an array of uint64_t's.
 * @param a		Pointer to first value.
 * @param b		Pointer to second value.
 * @return		Result of the comparison. */
static int frequency_compare(const void *a, const void *b) {
	return *(uint64_t *)a - *(uint64_t *)b;
}

/** Calculate a frequency multiple times and get the median of the results.
 * @param func		Function to call to get a frequency.
 * @return		Median of the results. */
static uint64_t calculate_frequency(uint64_t (*func)(void)) {
	uint64_t results[FREQUENCY_ATTEMPTS];
	size_t i;

	/* Get the frequencies. */
	for(i = 0; i < FREQUENCY_ATTEMPTS; i++) {
		results[i] = func();
	}

	/* Sort them in ascending order. */
	qsort(results, FREQUENCY_ATTEMPTS, sizeof(uint64_t), frequency_compare);

	/* Pick the median of the results. */
	return results[FREQUENCY_ATTEMPTS / 2];
}

/** Function to calculate the CPU frequency.
 * @return		Calculated frequency. */
static uint64_t calculate_cpu_frequency(void) {
	uint16_t shi, slo, ehi, elo, ticks;
	uint64_t start, end, cycles;

	/* First set the PIT to rate generator mode. */
	out8(0x43, 0x34);
	out8(0x40, 0xFF);
	out8(0x40, 0xFF);

	/* Wait for the cycle to begin. */
	do {
		out8(0x43, 0x00);
		slo = in8(0x40);
		shi = in8(0x40);
	} while(shi != 0xFF);

	/* Get the start TSC value. */
	start = rdtsc();

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the end TSC value. */
	end = rdtsc();

	/* Calculate the differences between the values. */
	cycles = end - start;
	ticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (cycles * PIT_FREQUENCY) / ticks;
}

/** Function to calculate the LAPIC timer frequency.
 * @return		Calculated frequency. */
static uint64_t calculate_lapic_frequency(void) {
	uint16_t shi, slo, ehi, elo, pticks;
	uint64_t end, lticks;

	/* First set the PIT to rate generator mode. */
	out8(0x43, 0x34);
	out8(0x40, 0xFF);
	out8(0x40, 0xFF);

	/* Wait for the cycle to begin. */
	do {
		out8(0x43, 0x00);
		slo = in8(0x40);
		shi = in8(0x40);
	} while(shi != 0xFF);

	/* Kick off the LAPIC timer. */
	lapic_mapping[LAPIC_REG_TIMER_INITIAL] = 0xFFFFFFFF;

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the current timer value. */
	end = lapic_mapping[LAPIC_REG_TIMER_CURRENT];

	/* Calculate the differences between the values. */
	lticks = 0xFFFFFFFF - end;
	pticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (lticks * 4 * PIT_FREQUENCY) / pticks;
}

/** Detect information about the current CPU.
 * @param cpu		CPU structure to store in. */
static void cpu_arch_init(kernel_args_cpu_arch_t *cpu) {
	uint32_t eax, ebx, ecx, edx, flags;
	uint32_t *ptr;
	size_t i, j;
	char *str;

	/* Initialise everything to zero to begin with. */
	memset(cpu, 0, sizeof(kernel_args_cpu_arch_t));

	/* Check if CPUID is supported - if we can change EFLAGS.ID, it is. */
	flags = x86_read_flags();
	x86_write_flags(flags ^ X86_FLAGS_ID);
	if((x86_read_flags() & X86_FLAGS_ID) != (flags & X86_FLAGS_ID)) {
		/* Get the highest supported standard level. */
		x86_cpuid(X86_CPUID_VENDOR_ID, &cpu->highest_standard, &ebx, &ecx, &edx);
		if(cpu->highest_standard >= X86_CPUID_FEATURE_INFO) {
			/* Get standard feature information, and then clear any */
			x86_cpuid(X86_CPUID_FEATURE_INFO, &eax, &ebx, &cpu->standard_ecx, &cpu->standard_edx);
			cpu->family = (eax >> 8) & 0x0f;
			cpu->model = (eax >> 4) & 0x0f;
			cpu->stepping = eax & 0x0f;

			/* If the CLFLUSH instruction is supported, get the
			 * cache line size. If it is not, a sensible default
			 * will be chosen later based on whether long mode is
			 * supported. */
			if(cpu->standard_edx & (1<<19)) {
				cpu->cache_alignment = ((ebx >> 8) & 0xFF) * 8;
			}
		}

		/* Get the highest supported extended level. */
		x86_cpuid(X86_CPUID_EXT_MAX, &cpu->highest_extended, &ebx, &ecx, &edx);
		if(cpu->highest_extended & (1<<31)) {
			if(cpu->highest_extended >= X86_CPUID_EXT_FEATURE) {
				/* Get extended feature information. */
				x86_cpuid(X86_CPUID_EXT_FEATURE, &eax, &ebx, &cpu->extended_ecx, &cpu->extended_edx);
			}

			if(cpu->highest_extended >= X86_CPUID_BRAND_STRING3) {
				/* Get brand information. */
				ptr = (uint32_t *)cpu->model_name;
				x86_cpuid(X86_CPUID_BRAND_STRING1, &ptr[0], &ptr[1], &ptr[2],  &ptr[3]);
				x86_cpuid(X86_CPUID_BRAND_STRING2, &ptr[4], &ptr[5], &ptr[6],  &ptr[7]);
				x86_cpuid(X86_CPUID_BRAND_STRING3, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

				/* Some CPUs right-justify the string... */
				str = cpu->model_name;
				i = 0; j = 0;
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

			if(cpu->highest_extended >= X86_CPUID_ADDRESS_SIZE) {
				/* Get address size information. */
				x86_cpuid(X86_CPUID_ADDRESS_SIZE, &eax, &ebx, &ecx, &edx);
				cpu->max_phys_bits = eax & 0xff;
				cpu->max_virt_bits = (eax >> 8) & 0xff;
			}
		} else {
			cpu->highest_extended = 0;
		}
	}

	/* Get a brand string if one wasn't found. */
	if(!cpu->model_name[0]) {
		/* TODO: Get this based on the family/model/stepping. */
		strcpy(cpu->model_name, "Unknown Model");
	}

	/* If the cache line size is not set, use a sane default based on
	 * whether the CPU supports long mode. */
	if(!cpu->cache_alignment) {
		cpu->cache_alignment = (cpu->extended_edx & (1<<29)) ? 64 : 32;
	}

	/* Same goes for address sizes. */
	if(!cpu->max_phys_bits) {
		cpu->max_phys_bits = 36;
	}
	if(!cpu->max_virt_bits) {
		cpu->max_virt_bits = (cpu->extended_edx & (1<<29)) ? 48 : 32;
	}

	/* Find out the CPU frequency. When running under QEMU the boot CPU's
	 * frequency is OK but the others will usually get rubbish, so as a
	 * workaround use the boot CPU's frequency on all CPUs under QEMU. */
	if(strncmp(cpu->model_name, "QEMU", 4) != 0 || booting_cpu == boot_cpu) {
		cpu->cpu_freq = calculate_frequency(calculate_cpu_frequency);
	} else {
		cpu->cpu_freq = boot_cpu->arch.cpu_freq;
	}

	/* Now that we have all information, update the feature set for all CPUs. */
	kernel_args->arch.standard_ecx &= cpu->standard_ecx;
	kernel_args->arch.standard_edx &= cpu->standard_edx;
	kernel_args->arch.extended_ecx &= cpu->extended_ecx;
	kernel_args->arch.extended_edx &= cpu->extended_edx;
	kernel_args->arch.cache_alignment = MAX(kernel_args->arch.cache_alignment, cpu->cache_alignment);
}

/** Initialise the local APIC.
 * @return		Whether the local APIC is present. */
bool cpu_lapic_init(void) {
	uint32_t *mapping;
	uint64_t base;

	if(!(booting_cpu->arch.standard_edx & (1<<9))) {
		return false;
	}

	/* Get the base address of the LAPIC mapping. If bit 11 is 0, the LAPIC
	 * is disabled. */
	base = x86_read_msr(X86_MSR_APIC_BASE);
	if(!(base & (1<<11))) {
		return false;
	} else if(booting_cpu->arch.standard_edx & (1<<21) && base & (1<<10)) {
		boot_error("CPU %u is in x2APIC mode", booting_cpu->id);
	}

	/* Store the mapping address, ensuring no CPUs have differing
	 * addresses. */
	mapping = (uint32_t *)((ptr_t)base & 0xFFFFF000);
	if(lapic_mapping) {
		if(lapic_mapping != mapping) {
			boot_error("CPUs have different LAPIC base addresses");
		}
	} else {
		lapic_mapping = mapping;
		kernel_args->arch.lapic_address = base & 0xFFFFF000;
	}

	/* Enable the LAPIC. */
	lapic_mapping[LAPIC_REG_SPURIOUS] = lapic_mapping[LAPIC_REG_SPURIOUS] | (1<<8);
	lapic_mapping[LAPIC_REG_TIMER_DIVIDER] = LAPIC_TIMER_DIV4;
	lapic_mapping[LAPIC_REG_LVT_TIMER] = (1<<16);

	/* Calculate LAPIC frequency. See comment about CPU frequency in QEMU,
	 * same applies here. */
	if(strncmp(booting_cpu->arch.model_name, "QEMU", 4) != 0 || booting_cpu == boot_cpu) {
		booting_cpu->arch.lapic_freq = calculate_frequency(calculate_lapic_frequency);
	} else {
		booting_cpu->arch.lapic_freq = boot_cpu->arch.lapic_freq;
	}
	return true;
}

/** Send an IPI.
 * @param dest		Destination Shorthand.
 * @param id		Destination local APIC ID (if APIC_IPI_DEST_SINGLE).
 * @param mode		Delivery Mode.
 * @param vector	Value of vector field. */
static void cpu_ipi(uint8_t dest, uint32_t id, uint8_t mode, uint8_t vector) {
	/* Write the destination ID to the high part of the ICR. */
	lapic_mapping[LAPIC_REG_ICR1] = id << 24;

	/* Send the IPI:
	 * - Destination Mode: Physical.
	 * - Level: Assert (bit 14).
	 * - Trigger Mode: Edge. */
	lapic_mapping[LAPIC_REG_ICR0] = (1<<14) | (dest << 18) | (mode << 8) | vector;

	/* Wait for the IPI to be sent (check Delivery Status bit). */
	while(lapic_mapping[LAPIC_REG_ICR0] & (1<<12)) {
		__asm__ volatile("pause");
	}
}

/** Boot a CPU.
 * @param cpu		CPU to boot. */
static void cpu_boot(kernel_args_cpu_t *cpu) {
	uint32_t delay;

	assert(!kernel_args->smp_disabled);
	assert(!kernel_args->arch.lapic_disabled);

	dprintf("cpu: booting CPU %" PRIu32 "...\n", cpu->id, cpu);
	booting_cpu = cpu;
	atomic_set(&ap_boot_wait, 0);

	/* Copy the trampoline code to 0x7000. */
	memcpy((void *)0x7000, __ap_trampoline_start, __ap_trampoline_end - __ap_trampoline_start);

	/* Allocate a new stack for the AP, marked as reclaimable. */
	ap_stack_ptr = phys_memory_alloc(KSTACK_SIZE, PAGE_SIZE, true) + PAGE_SIZE;

	/* Send an INIT IPI to the AP to reset its state and delay 10ms. */
	cpu_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_INIT, 0x00);
	spin(10000);

	/* Send a SIPI. The 0x07 argument specifies where to look for the
	 * bootstrap code, as the SIPI will start execution from 0x000VV000,
	 * where VV is the vector specified in the IPI. We don't do what the
	 * MP Specification says here because QEMU assumes that if a CPU is
	 * halted (even by the 'hlt' instruction) then it can accept SIPIs.
	 * If the CPU reaches the idle loop before the second SIPI is sent, it
	 * will fault. */
	cpu_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	spin(10000);

	/* If the CPU is up, then return. */
	if(atomic_get(&ap_boot_wait)) {
		return;
	}

	/* Send a second SIPI and then check in 10ms intervals to see if it
	 * has booted. If it hasn't booted after 5 seconds, fail. */
	cpu_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_SIPI, 0x07);
	for(delay = 0; delay < 5000000; delay += 10000) {
		if(atomic_get(&ap_boot_wait)) {
			return;
		}
		spin(10000);
	}

	boot_error("CPU %" PRIu32 " timed out while booting", cpu->id);
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to delay for. */
void spin(useconds_t us) {
	/* Work out when we will finish */
	uint64_t target = rdtsc() + ((boot_cpu->arch.cpu_freq / 1000000) * us);

	/* Spin until the target is reached. */
	while(rdtsc() < target) {
		__asm__ volatile("pause");
	}
}

/** Get the ID of the current CPU. */
uint32_t cpu_current_id(void) {
	return (kernel_args->arch.lapic_disabled) ? 0 : (lapic_mapping[LAPIC_REG_APIC_ID] >> 24);
}

/** Boot all CPUs.
 * @param entry		Entry function for the CPUs. */
void cpu_boot_all(void (*entry)(void)) {
	kernel_args_cpu_t *cpu;
	phys_ptr_t addr;

	ap_entry_func = entry;

	for(addr = kernel_args->cpus; addr; addr = cpu->next) {
		cpu = (kernel_args_cpu_t *)((ptr_t)addr);
		if(cpu->id == cpu_current_id()) {
			continue;
		}
		cpu_boot(cpu);
	}

	dprintf("cpu: detected %" PRIu32 " CPU(s):\n", kernel_args->cpu_count);

	for(addr = kernel_args->cpus; addr; addr = cpu->next) {
		cpu = (kernel_args_cpu_t *)((ptr_t)addr);

		dprintf(" cpu%" PRIu32 ": %s (family: %u, model: %u, stepping: %u)\n",
		        cpu->id, cpu->arch.model_name, cpu->arch.family,
		        cpu->arch.model, cpu->arch.stepping);
		dprintf("  cpu_freq:    %" PRIu64 "MHz\n", cpu->arch.cpu_freq / 1000 / 1000);
		if(!kernel_args->arch.lapic_disabled) {
			dprintf("  lapic_freq:  %" PRIu64 "MHz\n", cpu->arch.lapic_freq / 1000 / 1000);
		}
		dprintf("  clsize:      %d\n", cpu->arch.cache_alignment);
		dprintf("  phys_bits:   %d\n", cpu->arch.max_phys_bits);
		dprintf("  virt_bits:   %d\n", cpu->arch.max_virt_bits);
	}

	dprintf("cpu: feature set supported by all CPUs:\n");
	dprintf(" standard_ecx: 0x%" PRIx32 "\n", kernel_args->arch.standard_ecx);
	dprintf(" standard_edx: 0x%" PRIx32 "\n", kernel_args->arch.standard_edx);
	dprintf(" extended_ecx: 0x%" PRIx32 "\n", kernel_args->arch.extended_ecx);
	dprintf(" extended_edx: 0x%" PRIx32 "\n", kernel_args->arch.extended_edx);
}

/** Perform initialisation of the boot CPU. */
void cpu_init(void) {
	/* Set all of the bits in the feature masks to begin with, they will be
	 * cleared as necessary by cpu_arch_init(). */
	kernel_args->arch.standard_ecx = 0xFFFFFFFF;
	kernel_args->arch.standard_edx = 0xFFFFFFFF;
	kernel_args->arch.extended_ecx = 0xFFFFFFFF;
	kernel_args->arch.extended_edx = 0xFFFFFFFF;

	/* To begin with add the CPU with an ID of 0. It will be set correctly
	 * once we have set up the LAPIC. */
	booting_cpu = kargs_cpu_add(0);

	/* Detect CPU information. */
	cpu_arch_init(&booting_cpu->arch);
}

/** Entry function for an AP. */
void cpu_ap_entry(void) {
	cpu_arch_init(&booting_cpu->arch);

	/* It is assumed that all secondary CPUs have an LAPIC, seeing as they
	 * could not have been booted if they didn't. */
	if(!cpu_lapic_init()) {
		boot_error("CPU %" PRIu32 " APIC could not be enabled", booting_cpu->id);
	}

	atomic_inc(&ap_boot_wait);
	ap_entry_func();
}
