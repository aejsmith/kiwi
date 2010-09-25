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

#include <arch/io.h>
#include <arch/lapic.h>
#include <arch/page.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <console.h>
#include <fatal.h>
#include <init.h>
#include <time.h>

extern void syscall_arch_init(void);
extern void arch_reboot(void);

/** x86-specific early initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_premm_init(kernel_args_t *args) {
	cpu_features_init(&cpu_features, args->arch.standard_ecx, args->arch.standard_edx,
	                  args->arch.extended_ecx, args->arch.extended_edx);
	descriptor_init();
	intr_init();
	pat_init();
}

/** x86-specific second stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_postmm_init(kernel_args_t *args) {
#ifdef __x86_64__
	syscall_arch_init();
#else
	tss_init();
#endif
	lapic_init(args);
}

/** x86-specific initialisation for an AP.
 * @param args		Kernel arguments structure. */
void __init_text arch_ap_init(kernel_args_t *args) {
	descriptor_ap_init();
	pat_init();
	lapic_init(args);
#ifdef __x86_64__
	syscall_arch_init();
#endif
}

/** Reboot the system. */
void arch_reboot(void) {
	uint8_t val;

	/* Try the keyboard controller. */
	do {
		val = in8(0x64);
		if(val & (1<<0)) {
			in8(0x60);
		}
	} while(val & (1<<1));
	out8(0x64, 0xfe);
	spin(5000);

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
