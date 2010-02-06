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
 * @brief		AMD64 setjmp/longjmp functions.
 */

.section .text

.global setjmp
.type setjmp, @function
setjmp:
	/* Save return address */
	movq	0(%rsp), %rcx
	movq	%rcx, 0(%rdi)

	/* Save registers: rbp, rsp, rbx, r12, r13, r14 and r15 */
	movq	%rbp, 8(%rdi)
	movq	%rsp, 16(%rdi)
	movq	%rbx, 24(%rdi)
	movq	%r12, 32(%rdi)
	movq	%r13, 40(%rdi)
	movq	%r14, 48(%rdi)
	movq	%r15, 56(%rdi)

	/* Return 0 */
	movq	$0, %rax
	ret
.size setjmp, .-setjmp

.global longjmp
.type longjmp, @function
longjmp:
	/* Restore registers */
	movq	8(%rdi), %rbp
	movq	16(%rdi), %rsp
	movq	24(%rdi), %rbx
	movq	32(%rdi), %r12
	movq	40(%rdi), %r13
	movq	48(%rdi), %r14
	movq	56(%rdi), %r15

	/* Restore return address */
	movq	0(%rdi), %rcx
	movq	%rcx, 0(%rsp)

	/* Check return code */
	cmpl	$0, %esi
	jne	1f
	movl	$1, %esi
1:	movl	%esi, %eax
	ret
.size longjmp, .-longjmp
