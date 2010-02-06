/*
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
 * @brief		IA32 architecture core code.
 */

#include <arch/arch.h>
#include <arch/descriptor.h>
#include <arch/io.h>
#include <arch/lapic.h>
#include <arch/page.h>
#include <arch/syscall.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <proc/syscall.h>

#include <fatal.h>

/** System call handler function.
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
static bool syscall_intr_handler(unative_t num, intr_frame_t *frame) {
	bool state = intr_enable();

	frame->ax = syscall_handler((syscall_frame_t *)frame);
	intr_restore(state);
	return false;
}

/** IA32 architecture startup code.
 *
 * Initial startup code for the IA32 architecture, run before the memory
 * management subsystem is set up.
 *
 * @param data		Multiboot information pointer.
 */
void __init_text arch_premm_init(void *data) {
	descriptor_init();
	intr_init();
	cpu_arch_init(&curr_cpu->arch);
}

/** IA32 architecture startup code.
 *
 * Second stage startup code for the IA32 architecture, run after the memory
 * allocation subsystem is set up.
 */
void __init_text arch_postmm_init(void) {
	lapic_init();
	intr_register(SYSCALL_INT_NO, syscall_intr_handler);
}

/** IA32 architecture startup code.
 *
 * Third stage startup code for the IA32 architecture, unmaps the temporary
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
