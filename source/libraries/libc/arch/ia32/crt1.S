/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		IA32 application startup code.
 */

.section .text

/** Main program entry point. */
.global _start
.type _start, @function
_start:
	/* The pointer to the process_args_t structure is on the stack. */
	movl	4(%esp), %eax

	/* Set up the environment and run the main function. This function
	 * should exit with the main function's return code. */
	push	%eax
	call	libc_init
	addl	$4, %esp

	/* If we got here then something has broken (we should have exited).
	 * Die horribly. */
	movl	$0xDEADBEEF, %eax
	ud2a
1:	jmp	1b
.size _start, .-_start
