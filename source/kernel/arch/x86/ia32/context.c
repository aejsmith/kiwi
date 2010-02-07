/*
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
 * @brief		IA32 CPU context functions.
 */

#include <arch/stack.h>

#include <cpu/context.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <assert.h>
#include <console.h>
#include <fatal.h>

extern void __context_restore_frame(void);

/** Initialise a CPU context structure.
 *
 * Initialises a CPU context structure so that its instruction pointer points
 * to the given value and its stack pointer points to the top of the given
 * stack - assumes that the stack is KSTACK_SIZE bytes.
 *
 * @param ctx		Context to initialise.
 * @param ip		Instruction pointer.
 * @param stack		Base of stack.
 */
void context_init(context_t *ctx, ptr_t ip, unative_t *stack) {
	/* Ensure that everything is cleared to 0. */
	memset(ctx, 0, sizeof(context_t));

	/* Reserve 4 bytes for the return address to be placed on the stack
	 * by context_restore(). */
	ctx->sp = ((ptr_t)stack + KSTACK_SIZE) - STACK_DELTA;
	ctx->ip = ip;
}

/** Destroy a context structure.
 *
 * Frees up resources allocated for a CPU context structure.
 *
 * @param ctx		Context to destroy.
 */
void context_destroy(context_t *ctx) {
	/* Nothing happens. */
}

/** Restore a context to an interrupt frame.
 *
 * Modifies the given interrupt stack frame to return to a function which
 * will restore the given context structure. The interrupt frame must be
 * set to return to CPL0 - if it is not, a fatal error will be raised.
 *
 * @param ctx		Context structure to restore.
 * @param frame		Interrupt frame to modify.
 */
void context_restore_frame(context_t *ctx, intr_frame_t *frame) {
	assert((frame->cs & 3) == 0);

	/* Nasty stuff... if an interrupt occurs without a privelege level
	 * change then the stack pointer/segment will not be pushed/restored.
	 * To get the stack pointer set correctly we must return to a
	 * temporary function that restores the context properly. This deserves
	 * a massive "F*CK YOU" to Intel. */
	frame->ip = (unative_t)__context_restore_frame;
	frame->dx = (unative_t)ctx;
}
