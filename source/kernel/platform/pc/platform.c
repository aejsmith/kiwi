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
 * @brief		PC platform core code.
 */

#include <arch/cpu.h>
#include <arch/descriptor.h>
#include <arch/io.h>

#include <platform/pic.h>
#include <platform/pit.h>

#include <kernel.h>

/** PC platform first stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text platform_premm_init(kernel_args_t *args) {
	/* Nothing happens. */
}

/** PC platform second stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text platform_postmm_init(kernel_args_t *args) {
	/* Initialise interrupt handling and the timer. */
	pic_init();
	if(args->arch.lapic_disabled) {
		timer_device_set(&pit_timer_device);
	}
}

/** PC platform AP initialisation.
 * @param args		Kernel arguments structure. */
void __init_text platform_ap_init(kernel_args_t *args) {
	/* Nothing happens. */
}

/** Reboot the system. */
void platform_reboot(void) {
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

/** Power off the system. */
void platform_poweroff(void) {
	/* TODO. */
	cpu_halt();
}
