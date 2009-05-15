/* Kiwi IRQ handling code
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
 * @brief		IRQ handling code.
 *
 * @todo		Allow multiple handlers for an IRQ.
 */

#include <console/kprintf.h>

#include <cpu/intr.h>
#include <cpu/irq.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>

/** IRQ management operations. */
irq_ops_t *irq_ops;

/** Array of IRQ handling routines. */
static irq_handler_t irq_handlers[IRQ_COUNT] __aligned(8);

/** Registers an IRQ handler. */
int irq_register(unative_t num, irq_handler_t handler) {
	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	irq_handlers[num] = handler;
	return 0;
}

int irq_remove(unative_t num) {
	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	irq_ops->mask(num);
	irq_handlers[num] = NULL;
	return 0;
}

/** Mask an IRQ.
 *
 * Masks the given IRQ.
 *
 * @param num		IRQ to mask.
 */
int irq_mask(unative_t num) {
	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	irq_ops->mask(num);
	return 0;
}

/** Unmask an IRQ.
 *
 * Unmasks the given IRQ.
 *
 * @param num		IRQ to unmask.
 */
int irq_unmask(unative_t num) {
	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	irq_ops->unmask(num);
	return 0;
}

/** IRQ handler routine.
 *
 * Handles an IRQ from a device.
 *
 * @param num		Interrupt number.
 * @param regs		Pointer to CPU register structure.
 *
 * @return		Whether the current thread should be preempted.
 */
bool irq_handler(unative_t num, intr_frame_t *regs) {
	bool ret = false;

	assert(irq_ops);

	/* Work out the IRQ number. */
	num -= IRQ_BASE;
	assert(num < IRQ_COUNT);

	/* Execute any pre-handling function - on x86 this checks for
	 * spurious IRQs, etc. */
	if(irq_ops->pre_handle && !irq_ops->pre_handle(num, regs)) {
		return false;
	}

	/* Dispatch the IRQ - TODO. */
	if(irq_handlers[num]) {
		ret = irq_handlers[num](num, regs);
	} else {
		kprintf(LOG_DEBUG, "irq: received unknown IRQ%" PRIun "\n", num);
	}

	/* Perform post-handling actions (i.e. send an EOI to the interrupt
	 * controller). */
	if(irq_ops->post_handle) {
		irq_ops->post_handle(num, regs);
	}

	return ret;
}
