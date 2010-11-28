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

#include <arch/x86/cpu.h>
#include <arch/x86/lapic.h>
#include <arch/memory.h>

#include <cpu/cpu.h>

#include <lib/string.h>

#include <mm/kheap.h>

#include <console.h>
#include <kargs.h>
#include <kdbg.h>

/** Double fault handler stack for the boot CPU. */
static uint8_t boot_doublefault_stack[KSTACK_SIZE] __aligned(PAGE_SIZE);

/** Atomic variable for paused CPUs to wait on. */
atomic_t cpu_pause_wait = 0;

/** Whether cpu_halt_all() has been called. */
atomic_t cpu_halting_all = 0;

/** Feature set present on all CPUs. */
cpu_features_t cpu_features;

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

/** Fill in a CPU features structure.
 * @param features	Structure to fill in.
 * @Param standard_ecx	Standard features ECX value.
 * @Param standard_ecx	Standard features EDX value.
 * @Param extended_ecx	Extended features ECX value.
 * @Param extended_ecx	Extended features EDX value. */
void cpu_features_init(cpu_features_t *features, uint32_t standard_ecx, uint32_t standard_edx,
                       uint32_t extended_ecx, uint32_t extended_edx) {
	features->fpu = standard_edx & (1<<0);
	features->vme = standard_edx & (1<<1);
	features->de = standard_edx & (1<<2);
	features->pse = standard_edx & (1<<3);
	features->tsc = standard_edx & (1<<4);
	features->msr = standard_edx & (1<<5);
	features->pae = standard_edx & (1<<6);
	features->mce = standard_edx & (1<<7);
	features->cx8 = standard_edx & (1<<8);
	features->apic = standard_edx & (1<<9);
	features->sep = standard_edx & (1<<11);
	features->mtrr = standard_edx & (1<<12);
	features->pge = standard_edx & (1<<13);
	features->mca = standard_edx & (1<<14);
	features->cmov = standard_edx & (1<<15);
	features->pat = standard_edx & (1<<16);
	features->pse36 = standard_edx & (1<<17);
	features->psn = standard_edx & (1<<18);
	features->clfsh = standard_edx & (1<<19);
	features->ds = standard_edx & (1<<21);
	features->acpi = standard_edx & (1<<22);
	features->mmx = standard_edx & (1<<23);
	features->fxsr = standard_edx & (1<<24);
	features->sse = standard_edx & (1<<25);
	features->sse2 = standard_edx & (1<<26);
	features->ss = standard_edx & (1<<27);
	features->htt = standard_edx & (1<<28);
	features->tm = standard_edx & (1<<29);
	features->pbe = standard_edx & (1<<31);

	features->sse3 = standard_ecx & (1<<0);
	features->pclmulqdq = standard_ecx & (1<<1);
	features->dtes64 = standard_ecx & (1<<2);
	features->monitor = standard_ecx & (1<<3);
	features->dscpl = standard_ecx & (1<<4);
	features->vmx = standard_ecx & (1<<5);
	features->smx = standard_ecx & (1<<6);
	features->est = standard_ecx & (1<<7);
	features->tm2 = standard_ecx & (1<<8);
	features->ssse3 = standard_ecx & (1<<9);
	features->cnxtid = standard_ecx & (1<<10);
	features->fma = standard_ecx & (1<<12);
	features->cmpxchg16b = standard_ecx & (1<<13);
	features->xtpr = standard_ecx & (1<<14);
	features->pdcm = standard_ecx & (1<<15);
	features->pcid = standard_ecx & (1<<17);
	features->dca = standard_ecx & (1<<18);
	features->sse4_1 = standard_ecx & (1<<19);
	features->sse4_2 = standard_ecx & (1<<20);
	features->x2apic = standard_ecx & (1<<21);
	features->movbe = standard_ecx & (1<<22);
	features->popcnt = standard_ecx & (1<<23);
	features->tscd = standard_ecx & (1<<24);
	features->aes = standard_ecx & (1<<25);
	features->xsave = standard_ecx & (1<<26);
	features->osxsave = standard_ecx & (1<<27);
	features->avx = standard_ecx & (1<<28);

	features->syscall = extended_edx & (1<<11);
	features->xd = extended_edx & (1<<20);
	features->lmode = extended_edx & (1<<29);

	features->lahf = extended_ecx & (1<<0);
}

/** Initialise an x86 CPU structure.
 * @param cpu		CPU structure to fill in.
 * @param args		Kernel arguments structure for the CPU. */
void __init_text cpu_arch_init(cpu_t *cpu, kernel_args_cpu_arch_t *args) {
	/* Set the pointer back to the CPU structure for curr_cpu. */
	cpu->arch.cpu_ptr = cpu;

	/* Copy information from the kernel arguments. */
	cpu->arch.cpu_freq = args->cpu_freq;
	cpu->arch.lapic_freq = args->lapic_freq;
	memcpy(cpu->arch.model_name, args->model_name, sizeof(cpu->arch.model_name));
	cpu->arch.family = args->family;
	cpu->arch.model = args->model;
	cpu->arch.stepping = args->stepping;
	cpu->arch.max_phys_bits = args->max_phys_bits;
	cpu->arch.max_virt_bits = args->max_virt_bits;
	cpu->arch.cache_alignment = args->cache_alignment;
	cpu_features_init(&cpu->arch.features, args->standard_ecx, args->standard_edx,
	                  args->extended_ecx, args->extended_edx);

	/* Work out the cycles per µs. */
	cpu->arch.cycles_per_us = cpu->arch.cpu_freq / 1000000;

	/* Allocate a stack to use for double fault handling. */
	if(cpu == &boot_cpu) {
		cpu->arch.double_fault_stack = boot_doublefault_stack;
	} else {
		cpu->arch.double_fault_stack = kheap_alloc(KSTACK_SIZE, MM_FATAL);
	}
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
