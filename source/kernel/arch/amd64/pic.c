/* Kiwi x86 PIC code
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
 * @brief		Programmable Interrupt Controller (i8259) code.
 */

#include <arch/intr.h>
#include <arch/pic.h>
#include <arch/io.h>

#include <console/kprintf.h>

#include <cpu/irq.h>

#include <fatal.h>
#include <kdbg.h>

/** IRQ masks - disable all by default, apart from IRQ2 (cascade). */
static uint8_t pic_mask_master = 0xFB;
static uint8_t pic_mask_slave = 0xFF;

/** Pre-handling function - checks for spurious interrupts.
 * @param num		IRQ number.
 * @param frame		Interrupt stack frame.
 * @return		True if IRQ should be handled. */
static bool pic_pre_handle(unative_t num, intr_frame_t *frame) {
	/* Check for spurious IRQs. */
	if(num == 7) {
		/* Read the In-Service Register, check the high bit. */
		out8(0x23, 3);
		if((in8(0x20) & 0x80) == 0) {
			kprintf(LOG_DEBUG, "intr: spurious IRQ7 (master), ignoring...\n");
			return false;
		}
	} else if(num == 15) {
		/* Read the In-Service Register, check the high bit. */
		out8(0xA3, 3);
		if((in8(0xA0) & 0x80) == 0) {
			kprintf(LOG_DEBUG, "intr: spurious IRQ15 (slave), ignoring...\n");
			return false;
		}
	}

	return true;
}

/** Post-handling function - sends an EOI.
 * @param num		IRQ number.
 * @param frame		Interrupt stack frame. */
static void pic_post_handle(unative_t num, intr_frame_t *frame) {
	/* Acknowledge the IRQ by sending an EOI. IRQ >= 8 == slave. */
	if(num >= 8) {
		out8(PIC_SLAVE_COMMAND, PIC_COMMAND_EOI);
	}

	/* Must always send the EOI to the master controller. */
	out8(PIC_MASTER_COMMAND, PIC_COMMAND_EOI);
}

/** IRQ mask function.
 * @param num		IRQ to mask. */
static void pic_mask(unative_t num) {
	if(num >= 8) {
		pic_mask_slave |= (1<<(num - 8));
		out8(PIC_SLAVE_DATA, pic_mask_slave);
	} else {
		pic_mask_master |= (1<<num);
		out8(PIC_MASTER_DATA, pic_mask_master);
	}
}

/** IRQ unmask function.
 * @param num		IRQ to unmask. */
static void pic_unmask(unative_t num) {
	if(num >= 8) {
		pic_mask_slave &= ~(1<<(num - 8));
		out8(PIC_SLAVE_DATA, pic_mask_slave);
	} else {
		pic_mask_master &= ~(1<<num);
		out8(PIC_MASTER_DATA, pic_mask_master);
	}
}

/** PIC IRQ operations. */
static irq_ops_t pic_irq_ops = {
	.pre_handle = pic_pre_handle,
	.post_handle = pic_post_handle,
	.mask = pic_mask,
	.unmask = pic_unmask,
};

/** Initialize the PIC. */
void pic_init(void) {
	/* Send an initialization command to both PICs (ICW1). */
	out8(PIC_MASTER_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);
	out8(PIC_SLAVE_COMMAND, PIC_ICW1_INIT | PIC_ICW1_ICW4);

	/* Set the interrupt vectors to use (ICW2). */
	out8(PIC_MASTER_DATA, IRQ_BASE);
	out8(PIC_SLAVE_DATA, IRQ_BASE + 8);

	/* Set how the PICs are connected to each other (ICW3). */
	out8(PIC_MASTER_DATA, 0x04);
	out8(PIC_SLAVE_DATA, 0x02);

	/* Set other behaviour flags (ICW4). */
	out8(PIC_MASTER_DATA, PIC_ICW4_8086);
	out8(PIC_SLAVE_DATA, PIC_ICW4_8086);

	/* Set IRQ masks. */
	out8(PIC_MASTER_DATA, pic_mask_master);
	out8(PIC_SLAVE_DATA, pic_mask_slave);

	/* Set the IRQ operations structure. */
	irq_ops = &pic_irq_ops;
}
