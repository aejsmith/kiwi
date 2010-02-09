/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		x86 architecture initialisation functions.
 */

#include <arch/boot.h>
#include <boot/menu.h>
#include <kargs.h>

/** Add x86-specific options to the menu.
 * @param menu		Main menu.
 * @param options	Options menu. */
void arch_add_menu_options(menu_t *menu, menu_t *options) {
	menu_add_checkbox(options, "Disable Local APIC usage", &g_kernel_args->arch.lapic_disabled);
}

/** Perform early architecture initialisation. */
void arch_early_init() {
	idt_init();
}
