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

#include <console/kprintf.h>

#include <cpu/intr.h>

#include <proc/sched.h>

#include <sync/spinlock.h>

#include <assert.h>
#include <fatal.h>

/** Array of interrupt handling routines. This will be initialized to 0 so
 * any interrupts that do not have a handler registered will get picked up
 * by intr_handler(). */
static intr_handler_t intr_handlers[INTR_COUNT] __aligned(8);

/** Lock to protect handler array. */
static SPINLOCK_DECLARE(intr_handlers_lock);

/** Register an interrupt handler.
 *
 * Registers a handler to be called upon receipt of a certain interrupt. If
 * a handler exists for the interrupt then it will be overwritten.
 *
 * @param num		Interrupt number.
 * @param handler	Function pointer to handler routine.
 *
 * @return		Old handler address.
 */
intr_handler_t intr_register(unative_t num, intr_handler_t handler) {
	intr_handler_t old;

	assert(num < INTR_COUNT);

	spinlock_lock(&intr_handlers_lock, 0);
	old = intr_handlers[num];
	intr_handlers[num] = handler;
	spinlock_unlock(&intr_handlers_lock);

	return old;
}

/** Remove an interrupt handler.
 *
 * Unregisters an interrupt handler.
 *
 * @param num		Interrupt number.
 */
void intr_remove(unative_t num) {
	assert(num < INTR_COUNT);

	spinlock_lock(&intr_handlers_lock, 0);
	intr_handlers[num] = NULL;
	spinlock_unlock(&intr_handlers_lock);
}

/** Interrupt handler routine.
 *
 * Handles a CPU interrupt by looking up the handler routine in the handler
 * table and calling it.
 *
 * @param num		Interrupt number.
 * @param frame		Interrupt stack frame.
 */
void intr_handler(unative_t num, intr_frame_t *frame) {
	intr_handler_t handler = intr_handlers[num];
	intr_result_t ret;

	if(unlikely(handler == NULL)) {
		_fatal(frame, "Recieved unknown interrupt %" PRIun, num);
	} else {
		ret = handler(num, frame);
		if(ret == INTR_RESCHEDULE) {
			sched_yield();
		}
	}
}
