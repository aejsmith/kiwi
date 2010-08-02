/*
 * Copyright (C) 2009-2010 Alex Smith
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

/** Main program entry point. */
.global _start
.type _start, @function
_start:
	/* Call the real C library initialisation function. The libkernel
	 * initialisation code calls us with a pointer to the process argument
	 * block as the first argument, so this will be passed through. */
	callq	__libc_init

	/* If we got here then something has broken (we should have exited).
	 * Die horribly. */
	movq	$0xDEADBEEF, %rax
	ud2a
1:	jmp	1b
.size _start, .-_start
