/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
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
typedef enum irq_result {
	IRQ_UNHANDLED,		/**< Interrupt was not handled. */
	IRQ_HANDLED,		/**< Interrupt was handled. */
	IRQ_PREEMPT,		/**< Interrupt was handled, and the current thread should be preempted. */
	IRQ_RUN_THREAD,		/**< Interrupt was handled, and the threaded handler should be run. */
} irq_result_t;

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

/** IRQ top-half handler function type.
 * @return		Interrupt status code. */
typedef irq_result_t (*irq_top_t)(unative_t num, void *data, intr_frame_t *frame);

/** IRQ bottom-half handler function type.
 * @return		Interrupt status code. */
typedef void (*irq_bottom_t)(unative_t num, void *data);

extern irq_ops_t *irq_ops;

extern status_t irq_register(unative_t num, irq_top_t top, irq_bottom_t bottom, void *data);
extern status_t irq_unregister(unative_t num, irq_top_t top, irq_bottom_t bottom, void *data);

extern void irq_handler(unative_t num, intr_frame_t *frame);

extern void irq_init(void);

#endif /* __CPU_INTR_H */
