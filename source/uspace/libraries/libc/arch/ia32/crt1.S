/* Kiwi IA32 application startup code
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
 * @brief		IA32 application startup code.
 */

.section .text

/* Stack contents upon entry:
 *  -              0(%esp)	- Fake return address.
 *  -              4(%esp)	- Argument count.
 *  -              8(%esp)	- Start of argument pointer array.
 *  - 8+((argc+1)*4)(%esp)	- Start of environment pointer array.
 *  -            ...(%esp)	- Start of ELF auxilary vector.
 * We don't know the end of the environment array because we don't have a
 * count for it. Therefore we rely on it being NULL-terminated (which it
 * should be) to find the end of it.
 */
.global _start
.type _start, @function
_start:
#if 0
	movl	4(%esp), %edi		/* Argument count. */
	leal	8(%esp), %esi		/* Find argument vector. */

	movl	%edi, %edx		/* Work out the beginning of the */
	addl	$1, %edx		/* environment using the above */
	imul	$4, %edx		/* formula, and store the result */
	addl	%esi, %edx		/* in %edx. */

	movl	%edx, %ecx		/* Find the end of the environment */
1:	addl	$8, %ecx		/* array using the NULL-terminator. */
	cmpl	$0, (%ecx)		/* The auxilary information is after */
	jne	1b			/* this. Store the result in %rcx. */
	leal	4(%ecx), %ecx

	/* Push arguments to __libc_init and call it. This will set up
	 * the environment pointers, etc., call the main function and
	 * call exit with the correct exit code. */
	push	%ecx
	push	%edx
	push	%esi
	push	%edi
#endif
	call	__libc_init

	/* If we got here then something has broken (we should have exited).
	 * Die horribly. */
	hlt
1:	jmp	1b
.size _start, .-_start
