/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		AMD64 userspace entry points.
 */

.section .text

.hidden libkernel_init

/** Main entry point for a userspace application. */
.global __libkernel_entry
.type __libkernel_entry, @function
.hidden __libkernel_entry
__libkernel_entry:
	/* Get pointer to arguments structure and call the main function. */
	movq	8(%rsp), %rdi
	callq	libkernel_init

	/* Entry address is now in %rax, jump to it. */
	jmp	*%rax
.size __libkernel_entry, .-__libkernel_entry
