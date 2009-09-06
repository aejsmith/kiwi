/* Kiwi AMD64 architecture core code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		AMD64 architecture core code.
 */

#include <arch/arch.h>
#include <arch/descriptor.h>
#include <arch/io.h>
#include <arch/page.h>
#include <arch/x86/lapic.h>
#include <arch/x86/sysreg.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <fatal.h>

extern void __syscall_entry(void);

/** Set up SYSCALL/SYSRET support for AMD64. */
static void __init_text syscall_arch_init(void) {
	uint64_t fmask, lstar, star;

	/* Set System Call Enable (SCE) flag in EFER. */
	sysreg_msr_write(SYSREG_MSR_EFER, sysreg_msr_read(SYSREG_MSR_EFER) | SYSREG_EFER_SCE);

	/* Disable interrupts and clear direction flag upon entry. */
	fmask = SYSREG_FLAGS_IF | SYSREG_FLAGS_DF;

	/* Set system call entry address. */
	lstar = (uint64_t)__syscall_entry;

	/* Set segments for entry and returning. In 64-bit mode things happen
	 * as follows upon entry:
	 *  - CS is set to the value in IA32_STAR[47:32].
	 *  - SS is set to the value in IA32_STAR[47:32] + 8.
	 * Upon return to 64-bit mode, the following happens:
	 *  - CS is set to (the value in IA32_STAR[63:48] + 16).
	 *  - SS is set to (the value in IA32_STAR[63:48] + 8).
	 * Weird. This means that we have to have a specific GDT order to
	 * make things work. We set the SYSRET values below to the kernel DS,
	 * so that we get the correct segment (kernel DS + 16 = user CS, and
	 * kernel DS + 8 = user DS).
	 */
	star = ((uint64_t)(SEG_K_DS | 0x03) << 48) | ((uint64_t)SEG_K_CS << 32);

	/* Write everything out. */
	sysreg_msr_write(SYSREG_MSR_FMASK, fmask);
	sysreg_msr_write(SYSREG_MSR_LSTAR, lstar);
	sysreg_msr_write(SYSREG_MSR_STAR, star);

	kprintf(LOG_DEBUG, "syscall: set up SYSCALL MSRs on CPU %" PRIu32 ":\n", curr_cpu->id);
	kprintf(LOG_DEBUG, "  FMASK: 0x%" PRIx64 "\n", fmask);
	kprintf(LOG_DEBUG, "  LSTAR: 0x%" PRIx64 "\n", lstar);
	kprintf(LOG_DEBUG, "  STAR:  0x%" PRIx64 "\n", star);
}

/** AMD64 architecture startup code.
 *
 * Initial startup code for the AMD64 architecture, run before the memory
 * management subsystem is set up.
 *
 * @param data		Multiboot information pointer.
 */
void __init_text arch_premm_init(void *data) {
	descriptor_init();
	intr_init();
	cpu_arch_init(&curr_cpu->arch);
}

/** AMD64 architecture startup code.
 *
 * Second stage startup code for the AMD64 architecture, run after the memory
 * allocation subsystem is set up.
 */
void __init_text arch_postmm_init(void) {
	lapic_init();
	syscall_arch_init();
}

/** AMD64 architecture startup code.
 *
 * Third stage startup code for the AMD64 architecture, unmaps the temporary
 * identity mapping used during boot.
 */
void __init_text arch_final_init(void) {
	page_late_init();
}

/** Architecture initialisation for an AP. */
void __init_text arch_ap_init(void) {
	descriptor_ap_init();
	cpu_arch_init(&curr_cpu->arch);

	/* Initialise the LAPIC. */
	if(!lapic_init()) {
		fatal("LAPIC initialisation failed for CPU %" PRIu32 "\n", curr_cpu->id);
	}

	syscall_arch_init();
}

/** Reboot the system. */
void arch_reboot(void) {
	int i;

	/* Try the keyboard controller. */
	out8(0x64, 0xfe);
	for(i = 0; i < 10000000; i++) {
		__asm__ volatile("pause");
	}

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
