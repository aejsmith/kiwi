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

#include <arch/features.h>
#include <arch/io.h>
#include <arch/stack.h>
#include <arch/lapic.h>
#include <arch/sysreg.h>

#include <boot/console.h>
#include <boot/cpu.h>
#include <boot/memory.h>

#include <lib/string.h>

#include <assert.h>
#include <fatal.h>

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

extern char __ap_trampoline_start[], __ap_trampoline_end[];

/** Address of the local APIC. */
static volatile uint32_t *lapic_mapping = NULL;

/** Stack pointer for the booting AP. */
ptr_t ap_stack_ptr = 0;

/** Read the Time Stamp Counter.
 * @return		Value of the TSC. */
static inline uint64_t rdtsc() {
	uint32_t high, low;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return ((uint64_t)high << 32) | low;
}

/** Detect information about the current CPU.
 * @param cpu		CPU structure to store in. */
static void cpu_arch_init(kernel_args_cpu_arch_t *cpu) {
	uint32_t eax, ebx, ecx, edx, flags;
	uint16_t shi, slo, ehi, elo, ticks;
	uint64_t start, end, cycles;
	uint32_t *ptr;
	size_t i, j;
	char *str;

	/* Initialise everything to zero to begin with. */
	memset(cpu, 0, sizeof(kernel_args_cpu_arch_t));

	/* Check if CPUID is supported - if we can change EFLAGS.ID, it is
	 * supported. */
	flags = sysreg_flags_read();
	sysreg_flags_write(flags ^ SYSREG_FLAGS_ID);
	if((sysreg_flags_read() & SYSREG_FLAGS_ID) == (flags & SYSREG_FLAGS_ID)) {
		fatal("CPU %" PRIu32 " does not support CPUID", booting_cpu->id);
	}

	/* Get the highest supported standard level. */
	cpuid(CPUID_VENDOR_ID, &cpu->largest_standard, &ebx, &ecx, &edx);
	if(cpu->largest_standard >= CPUID_FEATURE_INFO) {
		/* Get standard feature information. */
		cpuid(CPUID_FEATURE_INFO, &eax, &ebx, &cpu->feat_ecx, &cpu->feat_edx);
		cpu->family = (eax >> 8) & 0x0f;
		cpu->model = (eax >> 4) & 0x0f;
		cpu->stepping = eax & 0x0f;

		/* If the CLFLUSH instruction is supported, set the cache line
		 * size. */
		if(cpu->feat_edx & (1<<0)) {
			cpu->cache_alignment = ((ebx >> 8) & 0xFF) * 8;
		}
	} else {
		fatal("CPU %" PRIu32 " does not support CPUID feature information",
		      booting_cpu->id);
	}

	/* Get the highest supported extended level. */
	cpuid(CPUID_EXT_MAX, &cpu->largest_extended, &ebx, &ecx, &edx);
	if(cpu->largest_extended & (1<<31)) {
		if(cpu->largest_extended >= CPUID_EXT_FEATURE) {
			/* Get extended feature information. */
			cpuid(CPUID_EXT_FEATURE, &eax, &ebx, &cpu->ext_ecx, &cpu->ext_edx);
		}

		if(cpu->largest_extended >= CPUID_BRAND_STRING3) {
			/* Get brand information. */
			ptr = (uint32_t *)cpu->model_name;
			cpuid(CPUID_BRAND_STRING1, &ptr[0], &ptr[1], &ptr[2],  &ptr[3]);
			cpuid(CPUID_BRAND_STRING2, &ptr[4], &ptr[5], &ptr[6],  &ptr[7]);
			cpuid(CPUID_BRAND_STRING3, &ptr[8], &ptr[9], &ptr[10], &ptr[11]);

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
	} else {
		cpu->largest_extended = 0;
	}

	/* Get a brand string if one wasn't found. */
	if(!cpu->model_name[0]) {
		/* TODO: Get this based on the model information. */
		strcpy(cpu->model_name, "Unknown Model");
	}

	/* If the cache line size is not set, use a sane default based on
	 * whether the CPU supports long mode. */
	if(!cpu->cache_alignment) {
		cpu->cache_alignment = CPU_HAS_LMODE(booting_cpu) ? 64 : 32;
	}

	/* Check that all required features are supported. */
	if(!CPU_HAS_FPU(booting_cpu) || !CPU_HAS_TSC(booting_cpu) ||
	   !CPU_HAS_PAE(booting_cpu) || !CPU_HAS_PGE(booting_cpu) ||
	   !CPU_HAS_FXSR(booting_cpu)) {
		fatal("CPU %" PRIu32 " does not support required features",
		      booting_cpu->id);
	}
#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(CPU_HAS_XD(booting_cpu)) {
		sysreg_msr_write(SYSREG_MSR_EFER, sysreg_msr_read(SYSREG_MSR_EFER) | SYSREG_EFER_NXE);
	}
#endif
	/* Shitty workaround: when running under QEMU the boot CPU's frequency
	 * is OK but the others will usually get rubbish. Use the boot CPU's
	 * frequency on all CPUs under QEMU. */
	if(strncmp(cpu->model_name, "QEMU", 4) == 0 && booting_cpu != boot_cpu) {
		cpu->cpu_freq = boot_cpu->arch.cpu_freq;
		return;
	}

	/* Find out the CPU frequency. First set the PIT to rate generator
	 * mode. */
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
	cpu->cpu_freq = (cycles * PIT_FREQUENCY) / ticks;
}

/** Initialise the local APIC.
 * @return		Whether the local APIC is present. */
static bool cpu_lapic_init(void) {
	uint16_t shi, slo, ehi, elo, pticks;
	uint64_t end, lticks;
	uint32_t *mapping;
	uint64_t base;

	if(!CPU_HAS_APIC(booting_cpu)) {
		return false;
	}

	/* Get the base address of the LAPIC mapping. If bit 11 is 0, the LAPIC
	 * is disabled. */
	base = sysreg_msr_read(SYSREG_MSR_APIC_BASE);
	if(!(base & (1<<11))) {
		return false;
	}

	/* Store the mapping address, ensuring no CPUs have differing
	 * addresses. */
	mapping = (uint32_t *)((ptr_t)base & 0xFFFFF000);
	if(lapic_mapping) {
		if(lapic_mapping != mapping) {
			fatal("CPUs have different LAPIC base addresses");
		}
	} else {
		lapic_mapping = mapping;
		kernel_args->arch.lapic_address = base & 0xFFFFF000;
	}

	/* Enable the LAPIC. */
	lapic_mapping[LAPIC_REG_SPURIOUS] = lapic_mapping[LAPIC_REG_SPURIOUS] | (1<<8);
	lapic_mapping[LAPIC_REG_TIMER_DIVIDER] = LAPIC_TIMER_DIV4;
	lapic_mapping[LAPIC_REG_LVT_TIMER] = (1<<16);

	/* Shitty workaround: see above. */
	if(strncmp(booting_cpu->arch.model_name, "QEMU", 4) == 0 && booting_cpu != boot_cpu) {
		booting_cpu->arch.bus_freq = boot_cpu->arch.bus_freq;
		return true;
	}

	/* Calculate the CPU's bus frequency, which is used to calculate timer
	 * counts. First set the PIT to rate generator mode. */
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
	booting_cpu->arch.bus_freq = (lticks * 4 * PIT_FREQUENCY) / pticks;
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

	fatal("CPU %" PRIu32 " timed out while booting", cpu->id);
}

/** Print out CPU information. */
static void cpu_print_info(void) {
	kernel_args_cpu_t *cpu;
	phys_ptr_t addr;

	dprintf("cpu: detected %" PRIu32 " CPU(s):\n", kernel_args->cpu_count);

	for(addr = kernel_args->cpus; addr; addr = cpu->next) {
		cpu = (kernel_args_cpu_t *)((ptr_t)addr);

		dprintf(" cpu%" PRIu32 ": %s (family: %u, model: %u, stepping: %u)\n",
		        cpu->id, cpu->arch.model_name, cpu->arch.family,
		        cpu->arch.model, cpu->arch.stepping);
		dprintf("  cpu_freq: %" PRIu64 "MHz\n", cpu->arch.cpu_freq / 1000 / 1000);
		if(!kernel_args->arch.lapic_disabled) {
			dprintf("  bus_freq: %" PRIu64 "MHz\n", cpu->arch.bus_freq / 1000 / 1000);
		}
		dprintf("  clsize:   %d\n", cpu->arch.cache_alignment);
	}
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to delay for. */
void spin(uint64_t us) {
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

/** Boot all CPUs. */
void cpu_boot_all(void) {
	kernel_args_cpu_t *cpu;
	phys_ptr_t addr;

	for(addr = kernel_args->cpus; addr; addr = cpu->next) {
		cpu = (kernel_args_cpu_t *)((ptr_t)addr);
		if(cpu->id == cpu_current_id()) {
			continue;
		}
		cpu_boot(cpu);
	}

	cpu_print_info();
}

/** Perform early CPU initialisation. */
void cpu_early_init(void) {
	/* To begin with add the CPU with an ID of 0. It will be set correctly
	 * once we have set up the LAPIC. */
	booting_cpu = kargs_cpu_add(0);

	/* Detect CPU information. */
	cpu_arch_init(&booting_cpu->arch);
}

/** Perform extra initialisation for the BSP. */
void cpu_postmenu_init(void) {
	/* Check if the LAPIC is available. */
	if(!kernel_args->arch.lapic_disabled && cpu_lapic_init()) {
		/* Set the real ID of the boot CPU. */
		booting_cpu->id = cpu_current_id();
		if(booting_cpu->id > kernel_args->highest_cpu_id) {
			kernel_args->highest_cpu_id = booting_cpu->id;
		}
	} else {
		/* Force SMP to be disabled if the boot CPU does not have a
		 * local APIC or if it has been manually disabled. */
		kernel_args->arch.lapic_disabled = true;
		kernel_args->smp_disabled = true;
	}
}

/** Perform AP initialisation. */
void cpu_ap_init(void) {
	cpu_arch_init(&booting_cpu->arch);
	if(!cpu_lapic_init()) {
		fatal("CPU %" PRIu32 " APIC could not be enabled", booting_cpu->id);
	}
}
