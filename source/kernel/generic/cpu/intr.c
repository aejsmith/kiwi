/* Kiwi hardware interrupt handling code
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
 * @brief		Hardware interrupt handling code.
 */

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <mm/malloc.h>

#include <sync/spinlock.h>

#include <types/list.h>

#include <assert.h>
#include <errors.h>
#include <fatal.h>

/** Structure describing a handler for an IRQ. */
typedef struct irq_handler {
	list_t header;			/**< List header. */
	irq_func_t func;		/**< Function to call. */
	void *data;			/**< Argument to pass to handler. */
} irq_handler_t;

/** An entry in the IRQ table. */
typedef struct irq {
	spinlock_t lock;		/**< Lock to protect handler list. */
	list_t handlers;		/**< List of handler structures. */
} irq_t;

/** Array of IRQ structures. */
static irq_t irq_table[IRQ_COUNT];

/** IRQ handling operations provided by architecture/platform. */
irq_ops_t *irq_ops;

/** Registers an IRQ handler.
 *
 * Registers a handler for an IRQ. The new handler will be appended to the
 * list of IRQ handlers (IRQ handlers are called in the order they are
 * registered in.
 *
 * @param num		IRQ number.
 * @param func		Function to call when IRQ is received.
 * @param data		Data argument to pass to the handler.
 *
 * @return		0 on success, negative error code on failure.
 */
int irq_register(unative_t num, irq_func_t func, void *data) {
	irq_handler_t *handler, *exist;
	bool enable;

	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	handler = kmalloc(sizeof(irq_handler_t), MM_SLEEP);
	list_init(&handler->header);
	handler->func = func;
	handler->data = data;

	spinlock_lock(&irq_table[num].lock, 0);

	/* Check if a handler exists with the same function/data. */
	LIST_FOREACH(&irq_table[num].handlers, iter) {
		exist = list_entry(iter, irq_handler_t, header);

		if(exist->func == func && exist->data == data) {
			spinlock_unlock(&irq_table[num].lock);
			kfree(handler);
			return -ERR_ALREADY_EXISTS;
		}
	}

	enable = list_empty(&irq_table[num].handlers);
	list_append(&irq_table[num].handlers, &handler->header);

	/* Enable it if the list was empty before. */
	if(enable && irq_ops->enable) {
		irq_ops->enable(num);
	}

	spinlock_unlock(&irq_table[num].lock);
	return 0;
}

/** Removes an IRQ handler.
 *
 * Removes a previously added handler for an IRQ. This function must be passed
 * the handler function and data argument the handler was originally registered
 * with in order to be able to find the correct handler to remove.
 *
 * @param num		IRQ number.
 * @param func		Function handler was registered with.
 * @param data		Data argument handler was registered with.
 *
 * @return		0 on success, negative error code on failure.
 */
int irq_unregister(unative_t num, irq_func_t func, void *data) {
	irq_handler_t *handler;

	if(num >= IRQ_COUNT) {
		return -ERR_PARAM_INVAL;
	}

	spinlock_lock(&irq_table[num].lock, 0);

	LIST_FOREACH(&irq_table[num].handlers, iter) {
		handler = list_entry(iter, irq_handler_t, header);

		if(handler->func == func && handler->data == data) {
			list_remove(&handler->header);

			/* Disable if list now empty. */
			if(list_empty(&irq_table[num].handlers) && irq_ops->disable) {
				irq_ops->disable(num);
			}

			spinlock_unlock(&irq_table[num].lock);
			kfree(handler);
			return 0;
		}
	}

	spinlock_unlock(&irq_table[num].lock);
	return -ERR_NOT_FOUND;
}

/** Hardware interrupt handler.
 *
 * Handles a hardware interrupt by running necessary handlers for it.
 *
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 *
 * @return		Interrupt status code.
 */
intr_result_t irq_handler(unative_t num, intr_frame_t *frame) {
	bool level, handled = false, schedule = false;
	irq_handler_t *handler;
	intr_result_t ret;

	assert(irq_ops);
	assert(irq_ops->mode);

	/* Work out the IRQ number. */
	num -= IRQ_BASE;
	assert(num < IRQ_COUNT);

	/* Execute any pre-handling function. */
	if(irq_ops->pre_handle && !irq_ops->pre_handle(num, frame)) {
		return INTR_HANDLED;
	}

	/* Get the trigger mode. */
	level = irq_ops->mode(num, frame);

	/* Call handlers for the IRQ. */
	LIST_FOREACH_SAFE(&irq_table[num].handlers, iter) {
		handler = list_entry(iter, irq_handler_t, header);

		ret = handler->func(num, handler->data, frame);

		if(ret == INTR_HANDLED) {
			handled = true;
		} else if(ret == INTR_RESCHEDULE) {
			schedule = true;
		}

		/* For edge-triggered interrupts we must invoke all handlers,
		 * because interrupt pulses from multiple devices can be merged
		 * if they occur close together. */
		if(level && ret != INTR_UNHANDLED) {
			break;
		}
	}

	/* Perform post-handling actions. */
	if(irq_ops->post_handle) {
		irq_ops->post_handle(num, frame);
	}

	return (schedule) ? INTR_RESCHEDULE : ((handled) ? INTR_HANDLED : INTR_UNHANDLED);
}

/** Initialize the IRQ handling system. */
void irq_init(void) {
	size_t i;

	for(i = 0; i < IRQ_COUNT; i++) {
		spinlock_init(&irq_table[i].lock, "irq_lock");
		list_init(&irq_table[i].handlers);
	}
}
