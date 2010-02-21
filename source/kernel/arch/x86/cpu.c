/*
 * Copyright (C) 2008-2010 Alex Smith
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

#include <arch/features.h>
#include <arch/lapic.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <console.h>
#include <kargs.h>
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
	cpu_t *cpu;

	atomic_set(&cpu_pause_wait, 1);

	LIST_FOREACH(&cpus_running, iter) {
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
	LIST_FOREACH(&cpus_running, iter) {
		cpu = list_entry(iter, cpu_t, header);
		if(cpu->id != cpu_current_id()) {
			lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_NMI, 0);
		}
	}
}

/** Cause a CPU to reschedule.
 * @param cpu		CPU to reschedule. */
void cpu_reschedule(cpu_t *cpu) {
	lapic_ipi(LAPIC_IPI_DEST_SINGLE, cpu->id, LAPIC_IPI_FIXED, LAPIC_VECT_RESCHEDULE);
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

/** Initialise an x86 CPU structure.
 * @param cpu		CPU structure to fill in.
 * @param args		Kernel arguments structure for the CPU. */
void __init_text cpu_arch_init(cpu_arch_t *cpu, kernel_args_cpu_arch_t *args) {
	/* Copy information from the kernel arguments. */
	cpu->cpu_freq = args->cpu_freq;
	cpu->lapic_freq = args->lapic_freq;
	memcpy(cpu->model_name, args->model_name, sizeof(cpu->model_name));
	cpu->family = args->family;
	cpu->model = args->model;
	cpu->stepping = args->stepping;
	cpu->cache_alignment = args->cache_alignment;
	cpu->largest_standard = args->largest_standard;
	cpu->feat_ecx = args->feat_ecx;
	cpu->feat_edx = args->feat_edx;
	cpu->largest_extended = args->largest_extended;
	cpu->ext_ecx = args->ext_ecx;
	cpu->ext_edx = args->ext_edx;

	/* Work out the cycles per µs. */
	cpu->cycles_per_us = cpu->cpu_freq / 1000000;
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

	for(i = 0; i <= cpu_id_max; i++) {
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
