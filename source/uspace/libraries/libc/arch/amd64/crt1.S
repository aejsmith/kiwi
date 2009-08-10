/* Kiwi AMD64 application startup code
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
 * @brief		AMD64 application startup code.
 */

.section .text

/* Stack contents upon entry:
 *  -               0(%rsp)	- Fake return address.
 *  -               8(%rsp)	- Argument count.
 *  -              16(%rsp)	- Start of argument pointer array.
 *  - 16+((argc+1)*8)(%rsp)	- Start of environment pointer array.
 *  -             ...(%rsp)	- Start of ELF auxilary vector.
 * We don't know the end of the environment array because we don't have a
 * count for it. Therefore we rely on it being NULL-terminated (which it
 * should be) to find the end of it.
 */
.global _start
.type _start, @function
_start:
	/* Get argument count and start of argument array. */
	movq	8(%rsp), %rdi
	leaq	16(%rsp), %rsi

	/* Work out the beginning of the environment array using the formula
	 * described above. */
	movq	%rdi, %rdx
	addq	$1, %rdx
	imul	$8, %rdx
	addq	%rsi, %rdx

	/* Find the end of the environment array using the NULL terminator. */
	movq	%rdx, %rcx
1:	addq	$8, %rcx
	cmpq	$0, (%rcx)
	jne	1b

	/* Auxilary information is stored after the NULL terminator. */
	leaq	8(%rcx), %rcx

	/* Arguments are all in the correct position:
	 *  %rdi = argc, %rsi = argv, %rdx = envp, %rcx = auxv
	 * Go ahead and call into the C library to do the dirty work of
	 * setting up the environment. This will also call the main function
	 * and exit with it's return code. */
	callq	__libc_init

	/* If we got here then something has broken (we should have exited).
	 * Die horribly. */
	hlt
1:	jmp	1b
.size _start, .-_start
