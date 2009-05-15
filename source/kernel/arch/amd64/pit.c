/* Kiwi x86 Programmable Interval Timer code
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		Programmable Interval Timer code.
 */

#include <arch/io.h>

#include <cpu/irq.h>

#include <time/timer.h>

/** Handle a PIT tick.
 * @param num		IRQ number.
 * @param regs		Register dump.
 * @return		Value from clock_tick(). */
static bool pit_handler(unative_t num, intr_frame_t *regs) {
	return clock_tick();
}

/** Enable the PIT. */
static void pit_enable(void) {
	uint16_t base;

	/* Set frequency (1000Hz) */
	base = 1193182L / 1000;
	out8(0x43, 0x36);
	out8(0x40, base & 0xFF);
	out8(0x40, base >> 8);

	irq_register(0, pit_handler);
	irq_unmask(0);
}

/** Disable the PIT. */
static void pit_disable(void) {
	irq_mask(0);
	irq_remove(0);
}

/** PIT clock source. */
clock_source_t pit_clock_source = {
	.name = "PIT",
	.len = 1000000000 / 1000,
	.type = CLOCK_PERIODIC,

	.enable = pit_enable,
	.disable = pit_disable,
};
