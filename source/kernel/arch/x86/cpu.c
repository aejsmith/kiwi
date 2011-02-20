/*
 * Copyright (C) 2008-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		x86 CPU management.
 */

#include <arch/io.h>
#include <arch/memory.h>

#include <x86/cpu.h>
#include <x86/lapic.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <mm/kheap.h>

#include <assert.h>
#include <console.h>
#include <kargs.h>
#include <kdbg.h>

/** Frequency of the PIT. */
#define PIT_FREQUENCY		1193182L

/** Number of times to get a frequency (must be odd). */
#define FREQUENCY_ATTEMPTS	9

/** Double fault handler stack for the boot CPU. */
static uint8_t boot_doublefault_stack[KSTACK_SIZE] __aligned(PAGE_SIZE);

/** Atomic variable for paused CPUs to wait on. */
atomic_t cpu_pause_wait = 0;

/** Whether cpu_halt_all() has been called. */
atomic_t cpu_halting_all = 0;

/** Feature set present on all CPUs. */
cpu_features_t cpu_features;

/** Comparison function for qsort() on an array of uint64_t's.
 * @param a		Pointer to first value.
 * @param b		Pointer to second value.
 * @return		Result of the comparison. */
static __init_text int frequency_compare(const void *a, const void *b) {
	return *(const uint64_t *)a - *(const uint64_t *)b;
}

/** Calculate a frequency multiple times and get the median of the results.
 * @param func		Function to call to get a frequency.
 * @return		Median of the results. */
static __init_text uint64_t calculate_frequency(uint64_t (*func)()) {
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
static __init_text uint64_t calculate_cpu_frequency(void) {
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
	start = x86_rdtsc();

	/* Wait for the high byte to drop to 128. */
	do {
		out8(0x43, 0x00);
		elo = in8(0x40);
		ehi = in8(0x40);
	} while(ehi > 0x80);

	/* Get the end TSC value. */
	end = x86_rdtsc();

	/* Calculate the differences between the values. */
	cycles = end - start;
	ticks = ((ehi << 8) | elo) - ((shi << 8) | slo);

	/* Calculate frequency. */
	return (cycles * PIT_FREQUENCY) / ticks;
}

/** Pause execution of other CPUs.
 *
 * Pauses execution of all CPUs other than the CPU that calls the function.
 * This is done using an NMI, so CPUs will be paused even if they have
 * interrupts disabled. Use cpu_resume_all() to resume CPUs after using this
 * function.
 */
void cpu_pause_all(void) {
	cpu_t *cpu;

	atomic_set(&cpu_pause_wait, 1);

	LIST_FOREACH(&running_cpus, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu->id != cpu_current_id()) {
			lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_NMI, 0);
		}
	}
}

/** Resume CPUs paused with cpu_pause_all(). */
void cpu_resume_all(void) {
	atomic_set(&cpu_pause_wait, 0);
}

/** Halt all other CPUs. */
void cpu_halt_all(void) {
	cpu_t *cpu;

	atomic_set(&cpu_halting_all, 1);

	/* Have to do this rather than just use LAPIC_IPI_DEST_ALL, because
	 * during early boot, secondary CPUs do not have an IDT set up so
	 * sending them an NMI IPI results in a triple fault. */
	LIST_FOREACH(&running_cpus, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu->id != cpu_current_id()) {
			lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_NMI, 0);
		}
	}
}

/** Get current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on. This function should
 * only be used in cases where the curr_cpu variable is unavailable or unsafe,
 * i.e. during thread switching.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_current_id(void) {
        return (cpu_id_t)lapic_id();
}

/** Detect CPU features/information.
 * @param cpu		Pointer to architecture CPU structure to fill in.
 *			Assumes that the structure is zeroed out. */
static __init_text void detect_cpu_features(cpu_arch_t *cpu) {
	uint32_t eax, ebx, ecx, edx;
	uint32_t *ptr;
	size_t i, j;
	char *str;

	/* Get the highest supported standard level. */
	x86_cpuid(X86_CPUID_VENDOR_ID, &cpu->highest_standard, &ebx, &ecx, &edx);
	if(cpu->highest_standard < X86_CPUID_FEATURE_INFO) {
		fatal("CPUID feature information is not supported");
	}

	/* Get standard feature information. */
	x86_cpuid(X86_CPUID_FEATURE_INFO, &eax, &ebx, &cpu->features.standard_ecx,
		&cpu->features.standard_edx);

	/* Save model information. */
	cpu->family = (eax >> 8) & 0x0f;
	cpu->model = (eax >> 4) & 0x0f;
	cpu->stepping = eax & 0x0f;

	/* If the CLFLUSH instruction is supported, get the cache line size.
	 * If it is not, a sensible default will be chosen later based on
	 * whether long mode is supported. */
	if(cpu->features.clfsh) {
		cpu->cache_alignment = ((ebx >> 8) & 0xFF) * 8;
	}

	/* Get the highest supported extended level. */
	x86_cpuid(X86_CPUID_EXT_MAX, &cpu->highest_extended, &ebx, &ecx, &edx);
	if(cpu->highest_extended & (1<<31)) {
		if(cpu->highest_extended >= X86_CPUID_EXT_FEATURE) {
			/* Get extended feature information. */
			x86_cpuid(X86_CPUID_EXT_FEATURE, &eax, &ebx, &cpu->features.extended_ecx,
				&cpu->features.extended_edx);
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

	/* Get a brand string if one wasn't found. */
	if(!cpu->model_name[0]) {
		/* TODO: Get this based on the family/model/stepping. */
		strcpy(cpu->model_name, "Unknown Model");
	}

	/* If the cache line size is not set, use a sane default based on
	 * whether the CPU supports long mode. */
	if(!cpu->cache_alignment) {
		cpu->cache_alignment = (cpu->features.lmode) ? 64 : 32;
	}

	/* Same goes for address sizes. */
	if(!cpu->max_phys_bits) {
		cpu->max_phys_bits = 32;
	}
	if(!cpu->max_virt_bits) {
		cpu->max_virt_bits = (cpu->features.lmode) ? 48 : 32;
	}
}

/** Detect and set up the current CPU.
 * @param cpu		CPU structure for the current CPU. */
__init_text void cpu_arch_init(cpu_t *cpu) {
	/* If this is the boot CPU, a double fault stack will not have been
	 * allocated. Use the pre-allocated one in this case. */
	if(cpu == &boot_cpu) {
		cpu->arch.double_fault_stack = boot_doublefault_stack;
	} else {
		// FIXME!!
		// FIXME!! just to make sure i remember to set this
		// FIXME!!
		assert(cpu->arch.double_fault_stack);
	}

	/* Initialise and load descriptor tables. */
	descriptor_init(cpu);

	/* Set the CPU structure back pointer, used for the curr_cpu pointer
	 * before the thread system is up. */
	cpu->arch.parent = cpu;

	/* Detect features for the CPU. */
	detect_cpu_features(&cpu->arch);

	/* If this is the boot CPU, copy features to the global features
	 * structure. Otherwise, check that the feature set matches the global
	 * features. We do not allow SMP configurations with different features
	 * on different CPUs. */
//#if CONFIG_SMP
	if(cpu == &boot_cpu) {
//#endif
		memcpy(&cpu_features, &cpu->arch.features, sizeof(cpu_features));

		/* Check for required features. */
		if(!cpu_features.fpu || !cpu_features.fxsr) {
			fatal("CPU does not support FPU/FXSR");
		} else if(!cpu_features.tsc) {
			fatal("CPU does not support TSC");
		} else if(!cpu_features.pge) {
			fatal("CPU does not support PGE");
		}
//#if CONFIG_SMP
	} else {
		if(memcmp(&cpu_features, &cpu->arch.features, sizeof(cpu_features)) != 0) {
			fatal("CPU %u has different feature set to boot CPU", cpu->id);
		}
	}
//#endif

	/* Find out the CPU frequency. When running under QEMU the boot CPU's
	 * frequency is OK but the others will usually get rubbish, so as a
	 * workaround use the boot CPU's frequency on all CPUs under QEMU. */
	if(strncmp(cpu->arch.model_name, "QEMU", 4) != 0 || cpu == &boot_cpu) {
		cpu->arch.cpu_freq = calculate_frequency(calculate_cpu_frequency);
	} else {
		cpu->arch.cpu_freq = boot_cpu.arch.cpu_freq;
	}

	/* Work out the cycles per µs. */
	cpu->arch.cycles_per_us = cpu->arch.cpu_freq / 1000000;

	/* Enable PGE/OSFXSR. */
	x86_write_cr4(x86_read_cr4() | X86_CR4_PGE | X86_CR4_OSFXSR);

	/* Set WP/NE/MP/TS in CR0 (Write Protect, Numeric Error, Monitor
	 * Coprocessor, Task Switch), and clear EM (Emulation). TS is set
	 * because we do not want the FPU to be enabled initially. */
	x86_write_cr0((x86_read_cr0() | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP | X86_CR0_TS) & ~X86_CR0_EM);

#if CONFIG_X86_NX
	/* Enable NX/XD if supported. */
	if(cpu_features.xd) {
                x86_write_msr(X86_MSR_EFER, x86_read_msr(X86_MSR_EFER) | X86_EFER_NXE);
        }
#endif
}

/** CPU information command for KDBG.
 * @param argc		Argument count.
 * @param argv		Argument array.
 * @return		KDBG_OK on success. */
int kdbg_cmd_cpus(int argc, char **argv) {
	size_t i;

	if(KDBG_HELP(argc, argv)) {
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all CPUs and information about them.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID   Freq (MHz) LAPIC Freq (MHz) Cache Align Model Name\n");
	kprintf(LOG_NONE, "==   ========== ================ =========== ==========\n");

	for(i = 0; i <= highest_cpu_id; i++) {
		if(cpus[i] == NULL) {
			continue;
		}

		kprintf(LOG_NONE, "%-4" PRIu32 " %-10" PRIu64 " %-16" PRIu64 " %-11d %s\n",
		        cpus[i]->id, cpus[i]->arch.cpu_freq / 1000000,
		        cpus[i]->arch.lapic_freq / 1000000, cpus[i]->arch.cache_alignment,
		        (cpus[i]->arch.model_name[0]) ? cpus[i]->arch.model_name : "Unknown");
	}

	return KDBG_OK;
}
