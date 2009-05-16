/* Kiwi PC platform core code
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

#include <platform/acpi.h>
#include <platform/multiboot.h>
#include <platform/platform.h>

/** External initialization functions. */
extern void console_late_init(void);

/** PC platform startup code.
 *
 * Initial startup code for the PC platform, run before the memory management
 * subsystem is set up.
 *
 * @param data		Multiboot information pointer.
 */
void platform_premm_init(void *data) {
	multiboot_premm_init((multiboot_info_t *)data);
}

/** PC platform startup code.
 *
 * Second stage startup code for the PC platform, run after the memory
 * management subsystem is set up.
 */
void platform_postmm_init(void) {
	acpi_init();
	multiboot_postmm_init();
	console_late_init();
}
