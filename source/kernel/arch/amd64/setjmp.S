/*
 * Copyright (C) 2011 Alex Smith
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
 * @brief		AMD64 non-local jump functions.
 */

#include <arch/memory.h>
#include <x86/asm.h>

/** Context offsets. */
#define JMP_OFF_IP		0	/**< RIP offset. */
#define JMP_OFF_SP		8	/**< RSP offset. */
#define JMP_OFF_BP		16	/**< RBP offset. */
#define JMP_OFF_BX		24	/**< RBX offset. */
#define JMP_OFF_R12		32	/**< R12 offset. */
#define JMP_OFF_R13		40	/**< R13 offset. */
#define JMP_OFF_R14		48	/**< R14 offset. */
#define JMP_OFF_R15		56	/**< R15 offset. */

FUNCTION_START(initjmp)
	movq	%rsi, JMP_OFF_IP(%rdi)

	/* Save the stack pointer, minus 16 for initial return address and to
	 * maintain ABI alignment requirements ((RSP - 8) % 16 == 0). */
	addq	%rcx, %rdx
	subq	$8, %rdx
	movq	%rdx, JMP_OFF_SP(%rdi)

	movq	$0, JMP_OFF_BP(%rdi)
	ret
FUNCTION_END(initjmp)

FUNCTION_START(setjmp)
	/* Save instruction/stack pointers. */
	movq	(%rsp), %rax
	movq	%rax, JMP_OFF_IP(%rdi)
	movq	%rsp, JMP_OFF_SP(%rdi)

	/* Save callee-save registers. */
	movq	%rbp, JMP_OFF_BP(%rdi)
	movq	%rbx, JMP_OFF_BX(%rdi)
	movq	%r12, JMP_OFF_R12(%rdi)
	movq	%r13, JMP_OFF_R13(%rdi)
	movq	%r14, JMP_OFF_R14(%rdi)
	movq	%r15, JMP_OFF_R15(%rdi)

	/* Return 0 from setjmp(). */
	xorl	%eax, %eax
	ret
FUNCTION_END(setjmp)

FUNCTION_START(longjmp)
	/* Restore new callee-save registers. */
	movq	JMP_OFF_R15(%rdi), %r15
	movq	JMP_OFF_R14(%rdi), %r14
	movq	JMP_OFF_R13(%rdi), %r13
	movq	JMP_OFF_R12(%rdi), %r12
	movq	JMP_OFF_BX(%rdi), %rbx
	movq	JMP_OFF_BP(%rdi), %rbp

	/* Restore new instruction/stack pointers. */
	movq	JMP_OFF_SP(%rdi), %rsp
	movq	JMP_OFF_IP(%rdi), %rax
	movq	%rax, (%rsp)

	/* Cause setjmp() to return the specified value (change 0 to 1 if given). */
	test	%esi, %esi
	jnz	1f
	inc	%esi
1:	movl	%esi, %eax
	ret
FUNCTION_END(longjmp)
