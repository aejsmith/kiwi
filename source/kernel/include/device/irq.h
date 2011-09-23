/*
 * Copyright (C) 2009-2011 Alex Smith
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

#ifndef __DEVICE_IRQ_H
#define __DEVICE_IRQ_H

#include <arch/intr.h>

#include <types.h>

/** IRQ handler return status. */
typedef enum irq_status {
	IRQ_UNHANDLED,		/**< Interrupt was not handled. */
	IRQ_HANDLED,		/**< Interrupt was handled. */
	IRQ_PREEMPT,		/**< Interrupt was handled, and the current thread should be preempted. */
	IRQ_RUN_THREAD,		/**< Interrupt was handled, and the threaded handler should be run. */
} irq_status_t;

/** IRQ trigger modes. */
typedef enum irq_mode {
	IRQ_MODE_LEVEL,		/**< Level-triggered. */
	IRQ_MODE_EDGE,		/**< Edge-triggered. */
} irq_mode_t;

/** IRQ controller structure. */
typedef struct irq_controller {
	/** Pre-handling function.
	 * @param num		IRQ number.
	 * @return		True if IRQ should be handled. */
	bool (*pre_handle)(unsigned num);

	/** Post-handling function.
	 * @param num		IRQ number. */
	void (*post_handle)(unsigned num);

	/** Get IRQ trigger mode.
	 * @param num		IRQ number.
	 * @return		Trigger mode of the IRQ. */
	irq_mode_t (*mode)(unsigned num);

	/** Enable an IRQ.
	 * @param num		IRQ number. */
	void (*enable)(unsigned num);

	/** Disable an IRQ.
	 * @param num		IRQ number. */
	void (*disable)(unsigned num);
} irq_controller_t;

/** IRQ top-half handler function type.
 * @param num		IRQ number.
 * @param data		Data pointer associated with the handler.
 * @return		IRQ status code. */
typedef irq_status_t (*irq_top_t)(unsigned num, void *data);

/** IRQ bottom-half handler function type.
 * @param num		IRQ number.
 * @param data		Data pointer associated with the handler. */
typedef void (*irq_bottom_t)(unsigned num, void *data);

extern status_t irq_register(unsigned num, irq_top_t top, irq_bottom_t bottom, void *data);
extern status_t irq_unregister(unsigned num, irq_top_t top, irq_bottom_t bottom, void *data);

extern void irq_handler(unsigned num);

extern void irq_init(irq_controller_t *ctrlr);

#endif /* __DEVICE_IRQ_H */
