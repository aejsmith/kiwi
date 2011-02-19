/*
 * Copyright (C) 2008-2010 Alex Smith
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		IA32 CPU context functions.
 */

#include <arch/x86/asm.h>
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
	movl	4(%esp), %edx

	/* Save instruction/stack pointers. */
	movl	(%esp), %eax
	movl	%eax, CONTEXT_OFF_IP(%edx)
	movl	%esp, CONTEXT_OFF_SP(%edx)

	/* Save callee-save registers. */
	movl	%ebp, CONTEXT_OFF_BP(%edx)
	movl	%ebx, CONTEXT_OFF_BX(%edx)
	movl	%edi, CONTEXT_OFF_DI(%edx)
	movl	%esi, CONTEXT_OFF_SI(%edx)

	/* Return false from context_save(). */
	xorl	%eax, %eax
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
	movl	4(%esp), %edx
	jmp	__context_restore_frame
FUNCTION_END(context_restore)

/** Helper for context_restore_frame(). Entered with the context pointer in edx. */
FUNCTION_START(__context_restore_frame)
	/* Restore new callee-save registers. */
	movl	CONTEXT_OFF_SI(%edx), %esi
	movl	CONTEXT_OFF_DI(%edx), %edi
	movl	CONTEXT_OFF_BX(%edx), %ebx
	movl	CONTEXT_OFF_BP(%edx), %ebp

	/* Restore new instruction/stack pointers. */
	movl	CONTEXT_OFF_SP(%edx), %esp
	movl	CONTEXT_OFF_IP(%edx), %eax
	movl	%eax, (%esp)

	/* Cause context_save() to return true. */
	xorl	%eax, %eax
	inc	%eax
	ret
FUNCTION_END(__context_restore_frame)
