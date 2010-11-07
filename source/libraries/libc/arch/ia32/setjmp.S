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
 * @brief		IA32 setjmp/longjmp functions.
 */

.section .text

/* int setjmp(jmp_buf env); */
.global setjmp
.type setjmp, @function
setjmp:
	movl	4(%esp), %edx

	/* Save return address */
	movl	0(%esp), %ecx
	movl	%ecx, 0(%edx)

	/* Save registers: ebp, esp, ebx, edi and esi */
	movl	%ebp, 4(%edx)
	movl	%esp, 8(%edx)
	movl	%ebx, 12(%edx)
	movl	%edi, 16(%edx)
	movl	%esi, 20(%edx)

	/* Return 0 */
	movl	$0, %eax
	ret
.size setjmp, .-setjmp

/* void longjmp(jmp_buf env, int val); */
.global longjmp
.type setjmp, @function
longjmp:
	movl	4(%esp), %edx
	movl	8(%esp), %eax

	/* Restore registers */
	movl	4(%edx), %ebp
	movl	8(%edx), %esp
	movl	12(%edx), %ebx
	movl	16(%edx), %edi
	movl	20(%edx), %esi

	/* Restore return address */
	movl	0(%edx), %ecx
	movl	%ecx, 0(%esp)

	/* Check return code */
	cmpl	$0, %eax
	je	1f
	ret
1:
	movl	$1, %eax
	ret
.size longjmp, .-longjmp
