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
 * @brief		PC platform core code.
 */

#include <arch/lapic.h>

#include <platform/pic.h>
#include <platform/pit.h>
#include <platform/platform.h>

#include <time/timer.h>

#include <fatal.h>

/** PC platform second stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text platform_postmm_init(kernel_args_t *args) {
	/* Initialise interrupt handling and the timer. */
	pic_init();
	if(args->arch.lapic_disabled) {
		timer_device_set(&pit_timer_device);
	}
}
