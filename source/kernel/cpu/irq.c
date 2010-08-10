/*
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

#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <proc/process.h>
#include <proc/thread.h>

#include <sync/semaphore.h>
#include <sync/spinlock.h>

#include <assert.h>
#include <console.h>
#include <fatal.h>
#include <status.h>

/** Structure describing a handler for an IRQ. */
typedef struct irq_handler {
	list_t header;			/**< List header. */

	unative_t num;			/**< IRQ number. */
	irq_top_t top;			/**< Top-half handler. */
	irq_bottom_t bottom;		/**< Bottom-half handler. */
	void *data;			/**< Argument to pass to handler. */

	thread_t *thread;		/**< Thread for deferred handling. */
	semaphore_t sem;		/**< Semaphore to wait for interrupts on. */
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

/** IRQ handler thread main loop.
 * @param _handler	Pointer to handler structure.
 * @param arg2		Unused. */
static void irq_thread(void *_handler, void *arg2) {
	irq_handler_t *handler = _handler;

	assert(handler->bottom);

	while(true) {
		semaphore_down(&handler->sem);

		/* If the list header is not attached the handler has been
		 * removed, destroy it and exit. */
		if(list_empty(&handler->header)) {
			kfree(handler);
			return;
		}

		handler->bottom(handler->num, handler->data);
	}
}

/** Registers an IRQ handler.
 *
 * Registers a handler for an IRQ. The new handler will be appended to the
 * list of IRQ handlers (IRQ handlers are called in the order they are
 * registered in. The top-half handler will be run when the IRQ occurs. If it
 * returns IRQ_RUN_THREAD, then the bottom-half handler will be executed inside
 * a dedicated handler thread. If no top-half handler is specified then the
 * bottom-half handler will always be run.
 *
 * @param num		IRQ number.
 * @param top		Top-half handler, called in interrupt context.
 * @param bottom	Bottom-half handler, run in a seperate thread if
 *			requested by the top-half handler.
 * @param data		Data argument to pass to the handlers.
 *
 * @return		Status code describing result of the operation.
 */
status_t irq_register(unative_t num, irq_top_t top, irq_bottom_t bottom, void *data) {
	irq_handler_t *handler, *exist;
	char name[THREAD_NAME_MAX];
	status_t ret;
	bool enable;

	if(num >= IRQ_COUNT || (!top && !bottom)) {
		return STATUS_INVALID_ARG;
	}

	handler = kmalloc(sizeof(irq_handler_t), MM_SLEEP);
	list_init(&handler->header);
	semaphore_init(&handler->sem, "irq_sem", 0);
	handler->num = num;
	handler->top = top;
	handler->bottom = bottom;
	handler->data = data;
	handler->thread = NULL;

	/* Create a handler thread if necessary. */
	if(handler->bottom) {
		sprintf(name, "irq-%" PRIun, num);
		ret = thread_create(name, kernel_proc, 0, irq_thread, handler, NULL, &handler->thread);
		if(ret != STATUS_SUCCESS) {
			kfree(handler);
			return ret;
		}
	}

	spinlock_lock(&irq_table[num].lock);

	/* Check if a handler exists with the same functions/data. */
	LIST_FOREACH(&irq_table[num].handlers, iter) {
		exist = list_entry(iter, irq_handler_t, header);

		if(exist->top == top && exist->bottom == bottom && exist->data == data) {
			spinlock_unlock(&irq_table[num].lock);
			if(handler->thread) {
				thread_destroy(handler->thread);
			}
			kfree(handler);
			return STATUS_ALREADY_EXISTS;
		}
	}

	enable = list_empty(&irq_table[num].handlers);
	list_append(&irq_table[num].handlers, &handler->header);

	/* Enable it if the list was empty before. */
	if(enable && irq_ops->enable) {
		irq_ops->enable(num);
	}

	spinlock_unlock(&irq_table[num].lock);

	/* Run the thread. */
	if(handler->thread) {
		thread_run(handler->thread);
	}
	return STATUS_SUCCESS;
}

/** Removes an IRQ handler.
 *
 * Removes a previously added handler for an IRQ. This function must be given
 * the handler functions/data argument the handler was originally registered
 * with in order to be able to find the correct handler to remove.
 *
 * @param num		IRQ number.
 * @param top		Top-half function handler was registered with.
 * @param bottom	Bottom-half function handler was registered with.
 * @param data		Data argument handler was registered with.
 *
 * @return		Status code describing result of the operation.
 */
status_t irq_unregister(unative_t num, irq_top_t top, irq_bottom_t bottom, void *data) {
	irq_handler_t *handler;

	if(num >= IRQ_COUNT) {
		return STATUS_INVALID_ARG;
	}

	spinlock_lock(&irq_table[num].lock);

	LIST_FOREACH(&irq_table[num].handlers, iter) {
		handler = list_entry(iter, irq_handler_t, header);

		if(handler->top != top || handler->bottom != bottom || handler->data != data) {
			continue;
		}

		list_remove(&handler->header);

		/* Disable if list now empty. */
		if(list_empty(&irq_table[num].handlers) && irq_ops->disable) {
			irq_ops->disable(num);
		}

		spinlock_unlock(&irq_table[num].lock);

		/* If the handler has a thread, we leave destruction of the
		 * structure up to the thread - it checks whether the list
		 * header is attached each time it is woken to determine if it
		 * should exit. */
		if(handler->thread) {
			semaphore_up(&handler->sem, 1);
		} else {
			kfree(handler);
		}
		return STATUS_SUCCESS;
	}

	spinlock_unlock(&irq_table[num].lock);
	return STATUS_NOT_FOUND;
}

/** Hardware interrupt handler.
 * @param num		CPU interrupt number.
 * @param frame		Interrupt stack frame.
 * @return		Whether to reschedule. */
bool irq_handler(unative_t num, intr_frame_t *frame) {
	bool level, schedule = false;
	irq_handler_t *handler;
	irq_result_t ret;

	assert(irq_ops);
	assert(irq_ops->mode);

	/* Work out the IRQ number. */
	num -= IRQ_BASE;
	assert(num < IRQ_COUNT);

	/* Execute any pre-handling function. */
	if(irq_ops->pre_handle && !irq_ops->pre_handle(num, frame)) {
		return false;
	}

	/* Get the trigger mode. */
	level = irq_ops->mode(num, frame);

	/* First see if any handlers with top-half functions take the IRQ. */
	LIST_FOREACH_SAFE(&irq_table[num].handlers, iter) {
		handler = list_entry(iter, irq_handler_t, header);

		if(handler->top) {
			ret = handler->top(num, handler->data, frame);
			if(ret == IRQ_RESCHEDULE) {
				schedule = true;
			} else if(ret == IRQ_RUN_THREAD) {
				assert(handler->thread);
				semaphore_up(&handler->sem, 1);
				schedule = true;
			}

			/* For edge-triggered interrupts we must invoke all
			 * handlers, because multiple interrupt pulses can be
			 * merged if they occur close together. */
			if(level && ret != IRQ_UNHANDLED) {
				goto out;
			}
		}
	}

	/* No top-half handlers took the IRQ, or the IRQ is edge-triggered.
	 * Run all handlers without top-half functions. */
	LIST_FOREACH_SAFE(&irq_table[num].handlers, iter) {
		handler = list_entry(iter, irq_handler_t, header);

		if(!handler->top) {
			assert(handler->thread);
			semaphore_up(&handler->sem, 1);
			schedule = true;
		}
	}
out:
	/* Perform post-handling actions. */
	if(irq_ops->post_handle) {
		irq_ops->post_handle(num, frame);
	}

	return schedule;
}

/** Initialise the IRQ handling system. */
void __init_text irq_init(void) {
	size_t i;

	for(i = 0; i < IRQ_COUNT; i++) {
		spinlock_init(&irq_table[i].lock, "irq_lock");
		list_init(&irq_table[i].handlers);
	}
}
