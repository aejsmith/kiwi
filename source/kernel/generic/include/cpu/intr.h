/* Kiwi interrupt handling code
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
 * @brief		Interrupt handling code.
 */

#ifndef __CPU_INTR_H
#define __CPU_INTR_H

#include <arch/intr.h>

#include <types.h>

/** Interrupt handler return status. */
typedef enum intr_result {
	INTR_UNHANDLED,		/**< Interrupt not handled, invoke next handler. */
	INTR_HANDLED,		/**< Interrupt was handled, should not invoke next handler. */
	INTR_RESCHEDULE,	/**< A thread switch should be performed. */
} intr_result_t;

/** IRQ management operations. */
typedef struct irq_ops {
	/** Pre-handling function.
	 * @param num		IRQ number.
	 * @param frame		Interrupt frame.
	 * @return		True if IRQ should be handled. */
	bool (*pre_handle)(unative_t num, intr_frame_t *frame);

	/** Post-handling function.
	 * @param num		IRQ number.
	 * @param frame		Interrupt frame. */
	void (*post_handle)(unative_t num, intr_frame_t *frame);

	/** Get trigger mode function.
	 * @param num		IRQ number.
	 * @param frame		Interrupt frame.
	 * @return		True if level-triggered, false if edge. */
	bool (*mode)(unative_t num, intr_frame_t *frame);

	/** IRQ enable function.
	 * @param num		IRQ number. */
	void (*enable)(unative_t num);

	/** IRQ disable function.
	 * @param num		IRQ number. */
	void (*disable)(unative_t num);
} irq_ops_t;

/** IRQ handler routine type.
 * @return		Interrupt status code. */
typedef intr_result_t (*irq_func_t)(unative_t num, void *data, intr_frame_t *frame);

extern irq_ops_t *irq_ops;

extern int irq_register(unative_t num, irq_func_t handler, void *data);
extern int irq_unregister(unative_t num, irq_func_t handler, void *data);

extern intr_result_t irq_handler(unative_t num, intr_frame_t *frame);

extern void irq_init(void);

#endif /* __CPU_INTR_H */
