/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		x86 CPU context functions.
 */

#include <arch/memory.h>

#include <cpu/context.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <assert.h>
#include <console.h>

#ifndef __x86_64__
extern void __context_restore_frame(void);
#endif

/** Initialise a CPU context.
 *
 * Initialises a CPU context structure so that its instruction pointer points
 * to the given value and its stack pointer points to the top of the given
 * stack - assumes that the stack is KSTACK_SIZE bytes.
 *
 * @param ctx		Context to initialise.
 * @param ip		Instruction pointer.
 * @param stack		Base of stack.
 */
void context_init(context_t *ctx, ptr_t ip, void *stack) {
	/* Ensure that everything is cleared to 0. */
	memset(ctx, 0, sizeof(context_t));

	/* Reserve space for the return address to be placed on the stack by
	 * context_restore(). */
	ctx->sp = ((ptr_t)stack + KSTACK_SIZE) - sizeof(unative_t);
	ctx->ip = ip;
}

/** Destroy a CPU context.
 * @param ctx		Context to destroy. */
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

#ifdef __x86_64__
	frame->ip = (unative_t)context_restore;
	frame->di = (unative_t)ctx;
#else
	/* Nasty stuff... if an interrupt occurs without a privelege level
	 * change then the stack pointer/segment will not be pushed/restored.
	 * To get the stack pointer set correctly we must return to a
	 * temporary function that restores the context properly. */
	frame->ip = (unative_t)__context_restore_frame;
	frame->dx = (unative_t)ctx;
#endif
}
