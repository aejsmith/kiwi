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
 */

#ifndef __CPU_IRQ_H
#define __CPU_IRQ_H

#include <arch/intr.h>

#include <types.h>

/** IRQ management operations. */
typedef struct irq_ops {
	/** Pre-handling function.
	 * @return	True if IRQ should be handled. */
	bool (*pre_handle)(unative_t num, intr_frame_t *frame);
	/** Post-handling function. */
	void (*post_handle)(unative_t num, intr_frame_t *frame);
	/** IRQ mask function. */
	void (*mask)(unative_t num);
	/** IRQ unmask function. */
	void (*unmask)(unative_t num);
} irq_ops_t;

/** IRQ handler routine type.
 * @return		True if the current thread should be preempted. */
typedef bool (*irq_handler_t)(unative_t num, intr_frame_t *frame);

extern irq_ops_t *irq_ops;

extern int irq_register(unative_t num, irq_handler_t handler);
extern int irq_remove(unative_t num);
extern int irq_mask(unative_t num);
extern int irq_unmask(unative_t num);

extern bool irq_handler(unative_t num, intr_frame_t *frame);

#endif /* __CPU_IRQ_H */
