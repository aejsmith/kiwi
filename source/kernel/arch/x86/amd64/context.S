/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		AMD64 CPU context functions.
 */

#include <arch/asm.h>
#include <arch/context.h>

/** Save the current CPU context.
 *
 * Saves the current CPU context to the given context structure. Only the
 * callee-save registers and instruction/stack pointers are saved.
 *
 * @param ctx		Context structure to save to.
 *
 * @return		True if returning from context_restore(), false if
 *			returning from context_save().
 */
FUNCTION_START(context_save)
	/* Save instruction/stack pointers. */
	movq	(%rsp), %rax
	movq	%rax, CONTEXT_OFF_IP(%rdi)
	movq	%rsp, CONTEXT_OFF_SP(%rdi)

	/* Save callee-save registers. */
	movq	%rbp, CONTEXT_OFF_BP(%rdi)
	movq	%rbx, CONTEXT_OFF_BX(%rdi)
	movq	%r12, CONTEXT_OFF_R12(%rdi)
	movq	%r13, CONTEXT_OFF_R13(%rdi)
	movq	%r14, CONTEXT_OFF_R14(%rdi)
	movq	%r15, CONTEXT_OFF_R15(%rdi)

	/* Return false from context_save(). */
	xorq	%rax, %rax
	ret
FUNCTION_END(context_save)

/** Restore a context structure.
 *
 * Restores the CPU context stored in the given context structure. If the
 * context was stored with context_save(), then execution will resume after
 * the context_save() call, with a return value of true. This function does not
 * return to its caller.
 *
 * @param ctx		Context structure to restore.
 */
FUNCTION_START(context_restore)
	/* Restore new callee-save registers. */
	movq	CONTEXT_OFF_R15(%rdi), %r15
	movq	CONTEXT_OFF_R14(%rdi), %r14
	movq	CONTEXT_OFF_R13(%rdi), %r13
	movq	CONTEXT_OFF_R12(%rdi), %r12
	movq	CONTEXT_OFF_BX(%rdi), %rbx
	movq	CONTEXT_OFF_BP(%rdi), %rbp

	/* Restore new instruction/stack pointers. */
	movq	CONTEXT_OFF_SP(%rdi), %rsp
	movq	CONTEXT_OFF_IP(%rdi), %rax
	movq	%rax, (%rsp)

	/* Cause context_save() to return true. */
	xorq	%rax, %rax
	incq	%rax
	ret
FUNCTION_END(context_restore)
