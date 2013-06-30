/*
 * Copyright (C) 2009-2011 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Programmable Interrupt Controller code.
 */

#include <arch/io.h>

#include <device/irq.h>

#include <pc/pic.h>

#include <assert.h>
#include <kernel.h>

/** IRQ masks - disable all by default, apart from IRQ2 (cascade). */
static uint8_t pic_mask_master = 0xFB;
static uint8_t pic_mask_slave = 0xFF;

/** Level-triggered interrupts. */
static uint16_t pic_level_triggered = 0;

/** Acknowledge a PIC interrupt.
 * @param num		IRQ number. */
static void pic_eoi(unsigned num) {
	if(num >= 8) {
		out8(PIC_SLAVE_COMMAND, PIC_COMMAND_EOI);
	}

	/* Must always send the EOI to the master controller. */
	out8(PIC_MASTER_COMMAND, PIC_COMMAND_EOI);
}

/** Pre-handling function.
 * @param num		IRQ number.
 * @return		True if IRQ should be handled. */
static bool pic_pre_handle(unsigned num) {
	assert(num < 16);

	/* Check for spurious IRQs. */
	if(num == 7) {
		/* Read the In-Service Register, check the high bit. */
		out8(0x23, 3);
		if((in8(0x20) & 0x80) == 0) {
			kprintf(LOG_DEBUG, "pic: spurious IRQ7 (master), ignoring...\n");
			return false;
		}
	} else if(num == 15) {
		/* Read the In-Service Register, check the high bit. */
		out8(0xA3, 3);
		if((in8(0xA0) & 0x80) == 0) {
			kprintf(LOG_DEBUG, "pic: spurious IRQ15 (slave), ignoring...\n");
			return false;
		}
	}

	/* Edge-triggered interrupts must be acked before we handle. */
	if(!(pic_level_triggered & (1 << num))) {
		pic_eoi(num);
	}

	return true;
}

/** Post-handling function.
 * @param num		IRQ number. */
static void pic_post_handle(unsigned num) {
	/* Level-triggered interrupts must be acked once all handlers have been
	 * run. */
	if(pic_level_triggered & (1 << num)) {
		pic_eoi(num);
	}
}

/** Get the trigger mode of an IRQ.
 * @param num		IRQ number.
 * @return		Trigger mode of the IRQ. */
static irq_mode_t pic_mode(unsigned num) {
	return (pic_level_triggered & (1 << num));
}

/** Enable an IRQ.
 * @param num		IRQ to enable. */
static void pic_enable(unsigned num) {
	assert(num < 16);

	if(num >= 8) {
		pic_mask_slave &= ~(1 << (num - 8));
		out8(PIC_SLAVE_DATA, pic_mask_slave);
	} else {
		pic_mask_master &= ~(1<<num);
		out8(PIC_MASTER_DATA, pic_mask_master);
	}
}

/** Disable an IRQ.
 * @param num		IRQ to disable. */
static void pic_disable(unsigned num) {
	assert(num < 16);

	if(num >= 8) {
		pic_mask_slave |= (1<<(num - 8));
		out8(PIC_SLAVE_DATA, pic_mask_slave);
	} else {
		pic_mask_master |= (1<<num);
		out8(PIC_MASTER_DATA, pic_mask_master);
	}
}

/** PIC IRQ operations. */
static irq_controller_t pic_irq_controller = {
	.pre_handle = pic_pre_handle,
	.post_handle = pic_post_handle,
	.mode = pic_mode,
	.enable = pic_enable,
	.disable = pic_disable,
};

/** Initialize the PIC. */
__init_text void pic_init(void) {
	/* Send an initialization command to both PICs (ICW1). */
	out8(PIC_MASTER_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
	out8(PIC_SLAVE_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);

	/* Set the interrupt vectors to use (ICW2). */
	out8(PIC_MASTER_DATA, 32);
	out8(PIC_SLAVE_DATA, 32 + 8);

	/* Set how the PICs are connected to each other (ICW3). */
	out8(PIC_MASTER_DATA, 0x04);
	out8(PIC_SLAVE_DATA, 0x02);

	/* Set other behaviour flags (ICW4). */
	out8(PIC_MASTER_DATA, PIC_ICW4_8086);
	out8(PIC_SLAVE_DATA, PIC_ICW4_8086);

	/* Set initial IRQ masks. */
	out8(PIC_MASTER_DATA, pic_mask_master);
	out8(PIC_SLAVE_DATA, pic_mask_slave);

	/* Get the trigger modes. */
	pic_level_triggered = (in8(PIC_SLAVE_ELCR) << 8) | in8(PIC_MASTER_ELCR);

	/* Initialize the IRQ handling system. */
	irq_init(&pic_irq_controller);
}
