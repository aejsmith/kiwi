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
 * @brief		x86 architecture core code.
 */

#include <arch/lapic.h>
#include <arch/page.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <console.h>
#include <kernel.h>
#include <time.h>

extern void syscall_arch_init(void);

/** x86-specific early initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_premm_init(kernel_args_t *args) {
	cpu_features_init(&cpu_features, args->arch.standard_ecx,
	                  args->arch.standard_edx, args->arch.extended_ecx,
	                  args->arch.extended_edx);
	descriptor_init(&boot_cpu);
	intr_init();
	pat_init();
}

/** x86-specific second stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_postmm_init(kernel_args_t *args) {
#ifdef __x86_64__
	syscall_arch_init();
#else
	/* Set the correct CR3 value in the double fault TSS. When the TSS is
	 * set up by cpu_arch_init(), we are still on the PDP set up by the
	 * bootloader. */
	curr_cpu->arch.double_fault_tss.cr3 = x86_read_cr3();
#endif
	lapic_init(args);
}

/** x86-specific initialisation for an AP.
 * @param args		Kernel arguments structure.
 * @param cpu		CPU structure for the AP. */
void __init_text arch_ap_init(kernel_args_t *args, cpu_t *cpu) {
	descriptor_init(cpu);
	pat_init();
	lapic_init(args);
#ifdef __x86_64__
	syscall_arch_init();
#endif
}
