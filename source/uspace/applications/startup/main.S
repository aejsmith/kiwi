/* Kiwi userspace startup application
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
 * @brief		Userspace startup application.
 */

.global _start
.type _start, @function
_start:
#if __amd64__
	movq	$0, %rax
	movq	$1337, %rdi
	syscall
	movq	$0, %rax
	movq	$42, %rdi
	syscall
1:	jmp	1b
#else
	movl	$0, %eax
	movl	$1337, %edi
	int	$0x80
	movl	$0, %eax
	movl	$42, %edi
	int	$0x80
1:	jmp	1b
#endif
.size _start, .-_start
