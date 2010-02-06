/* Kiwi IA32 CPU context functions
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

#include <arch/context.h>

.global context_save
.global context_restore
.global __context_restore_frame

/** Save a context structure.
 *
 * Saves the current CPU context to the given context structure. Only the
 * callee-save registers and instruction/stack pointers are saved.
 *
 * @param ctx		Context structure to save to.
 *
 * @return		0 if returning from context_save(), 1 if returning
 *			from context_restore().
 */
context_save:
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

	/* Return 0 from context_save(). */
	xorl	%eax, %eax
	ret
.size context_save, .-context_save

/** Restore a context structure.
 *
 * Restores the CPU context stored in the given context structure. If the
 * context was stored with context_save(), then execution will resume after
 * the context_save() call, with a return value of 1. This function does not
 * return to its caller.
 *
 * @param ctx		Context structure to restore.
 */
context_restore:
	movl	4(%esp), %edx

	/* Restore new callee-save registers. */
	movl	CONTEXT_OFF_SI(%edx), %esi
	movl	CONTEXT_OFF_DI(%edx), %edi
	movl	CONTEXT_OFF_BX(%edx), %ebx
	movl	CONTEXT_OFF_BP(%edx), %ebp

	/* Restore new instruction/stack pointers. */
	movl	CONTEXT_OFF_SP(%edx), %esp
	movl	CONTEXT_OFF_IP(%edx), %eax
	movl	%eax, (%esp)

	/* Cause context_save() to return with 1. */
	xorl	%eax, %eax
	inc	%eax
	ret
.size context_restore, .-context_restore

/** Helper for context_restore_frame(). Entered with the context pointer in edx. */
__context_restore_frame:
	/* Restore new callee-save registers. */
	movl	CONTEXT_OFF_SI(%edx), %esi
	movl	CONTEXT_OFF_DI(%edx), %edi
	movl	CONTEXT_OFF_BX(%edx), %ebx
	movl	CONTEXT_OFF_BP(%edx), %ebp

	/* Restore new instruction/stack pointers. */
	movl	CONTEXT_OFF_SP(%edx), %esp
	movl	CONTEXT_OFF_IP(%edx), %eax
	movl	%eax, (%esp)

	/* Cause context_save() to return with 1. */
	xorl	%eax, %eax
	inc	%eax
	ret
.size __context_restore_frame, .-__context_restore_frame
