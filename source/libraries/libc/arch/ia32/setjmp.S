/*
 * Copyright (C) 2009 Alex Smith
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
.type longjmp, @function
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
