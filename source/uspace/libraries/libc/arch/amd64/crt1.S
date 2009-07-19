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
#if 0
	movq	8(%rsp), %rdi		/* Argument count. */
	leaq	16(%rsp), %rsi		/* Find argument vector. */

	movq	%rdi, %rdx		/* Work out the beginning of the */
	addq	$1, %rdx		/* environment using the above */
	imul	$8, %rdx		/* formula, and store the result */
	addq	%rsi, %rdx		/* in %rdx. */

	movq	%rdx, %rcx		/* Find the end of the environment */
1:	addq	$8, %rcx		/* array using the NULL-terminator. */
	cmpq	$0, (%rcx)		/* The auxilary information is after */
	jne	1b			/* this. Store the result in %rcx. */
	leaq	8(%rcx), %rcx

	/* Arguments are all in the correct position:
	 *  %rdi = argc, %rsi = argv, %rdx = envp, %rcx = auxv
	 * Go ahead and call into the C library to do the dirty work of
	 * setting up the environment. This will also call the main function
	 * and exit with it's return code. */
#endif
	callq	__libc_init

	/* If we got here then something has broken (we should have exited).
	 * Die horribly. */
	hlt
1:	jmp	1b
.size _start, .-_start
