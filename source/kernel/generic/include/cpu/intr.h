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
	INTR_HANDLED,		/**< Interrupt was handled, no extra action required. */
	INTR_RESCHEDULE,	/**< A thread switch should be performed. */
} intr_result_t;

/** Interrupt handler routine type.
 * @return		Interrupt status code (see above). */
typedef intr_result_t (*intr_handler_t)(unative_t num, intr_frame_t *frame);

extern intr_handler_t intr_register(unative_t num, intr_handler_t handler);
extern void intr_remove(unative_t num);

extern void intr_handler(unative_t num, intr_frame_t *frame);

#endif /* __CPU_INTR_H */
