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

#include <arch/x86/features.h>
#include <arch/x86/lapic.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <kdbg.h>

/** Atomic variable for paused CPUs to wait on. */
atomic_t cpu_pause_wait = 0;

/** Whether cpu_halt_all() has been called. */
atomic_t cpu_halting_all = 0;

/** Pause execution of other CPUs.
 *
 * Pauses execution of all CPUs other than the CPU that calls the function.
 * This is done using an NMI, so CPUs will be paused even if they have
 * interrupts disabled. Use cpu_resume_all() to resume CPUs after using this
 * function.
 */
void cpu_pause_all(void) {
	atomic_set(&cpu_pause_wait, 1);
	lapic_ipi(LAPIC_IPI_DEST_ALL, 0, LAPIC_IPI_NMI, 0);
}

/** Resume paused CPUs.
 *
 * Resumes execution of all other CPUs that have been paused using
 * cpu_pause_all().
 */
void cpu_resume_all(void) {
	atomic_set(&cpu_pause_wait, 0);
}

/** Halt all other CPUs.
 *
 * Halts execution of all other CPUs other than the CPU that calls the
 * function. The CPUs will not be able to resume after being halted.
 */
void cpu_halt_all(void) {
	atomic_set(&cpu_halting_all, 1);
	lapic_ipi(LAPIC_IPI_DEST_ALL, 0, LAPIC_IPI_NMI, 0);
}

/** Cause a CPU to reschedule.
 *
 * Causes the specified CPU to perform a thread switch.
 *
 * @param cpu		CPU to reschedule.
 */
void cpu_reschedule(cpu_t *cpu) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_FIXED, LAPIC_VECT_RESCHEDULE);
}

/** Get current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on. This function should
 * only be used in cases where the curr_cpu variable is unavailable, i.e.
 * during thread switching. Normally, you should use curr_cpu->id instead.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_current_id(void) {
        return (cpu_id_t)lapic_id();
}

/** Initialize an x86 CPU information structure.
 *
 * Fills in the given x86 CPU information structure with information about
 * the current CPU.
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
		kprintf(LOG_NONE, "Usage: %s\n\n", argv[0]);

		kprintf(LOG_NONE, "Prints a list of all CPUs and information about them.\n");
		return KDBG_OK;
	}

	kprintf(LOG_NONE, "ID   State    Model Name\n");
	kprintf(LOG_NONE, "==   =====    ==========\n");

	for(i = 0; i <= cpu_id_max; i++) {
		if(cpus[i] == NULL) {
			continue;
		}

		kprintf(LOG_NONE, "%-4" PRIu32 " ", cpus[i]->id);
		switch(cpus[i]->state) {
		case CPU_DISABLED:	kprintf(LOG_NONE, "Disabled "); break;
		case CPU_DOWN:		kprintf(LOG_NONE, "Down     "); break;
		case CPU_RUNNING:	kprintf(LOG_NONE, "Running  "); break;
		default:		kprintf(LOG_NONE, "Bad      "); break;
		}
		kprintf(LOG_NONE, "%s\n", (cpus[i]->arch.model_name[0]) ? cpus[i]->arch.model_name : "Unknown");
	}

	return KDBG_OK;
}
