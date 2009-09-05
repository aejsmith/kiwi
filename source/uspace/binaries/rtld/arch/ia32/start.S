/* Kiwi IA32 RTLD startup function
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
 * @brief		IA32 RTLD startup function.
 */

.section .text

/** Entry point for the RTLD. */
.global __rtld_entry
.type __rtld_entry, @function
__rtld_entry:
	/* Get pointer to arguments structure and call the main function. */
	movl	8(%esp), %eax
	push	%eax
	call	rtld_main
	addl	$4, %esp

	/* Entry address is now in %eax, jump to it. */
	jmp	*%eax
.size __rtld_entry, .-__rtld_entry
