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
#include <arch/asm.h>
#include <arch/defs.h>
#include <arch/descriptor.h>
#include <arch/features.h>
#include <arch/io.h>
#include <arch/lapic.h>
#include <arch/page.h>

#include <cpu/cpu.h>

#include <fatal.h>

/** AMD64 architecture startup code.
 *
 * Initial startup code for the AMD64 architecture, run before the memory
 * management subsystem is set up.
 *
 * @param data		Multiboot information pointer.
 */
void arch_premm_init(void *data) {
	descriptor_init();
	cpu_arch_init(&curr_cpu->arch);
}

/** AMD64 architecture startup code.
 *
 * Second stage startup code for the AMD64 architecture, run after the memory
 * allocation subsystem is set up.
 */
void arch_postmm_init(void) {
	lapic_init();
}

/** AMD64 architecture startup code.
 *
 * Third stage startup code for the AMD64 architecture, unmaps the temporary
 * identity mapping used during boot.
 */
void arch_final_init(void) {
	page_late_init();
}

#if CONFIG_SMP
/** Architecture initialization for an AP. */
void arch_ap_init(void) {
	descriptor_ap_init();
	cpu_arch_init(&curr_cpu->arch);

	/* Initialize the LAPIC. */
	if(!lapic_init()) {
		fatal("LAPIC initialization failed for CPU %" PRIu32 "\n", curr_cpu->id);
	}
}
#endif

/** Reboot the system. */
void arch_reboot(void) {
	int i;

	/* Try the keyboard controller. */
	out8(0x64, 0xfe);
	for(i = 0; i < 10000000; i++) {
		spin_loop_hint();
	}

	/* Fall back on a triple fault. */
	lidt(0, 0);
	__asm__ volatile("ud2");
}
