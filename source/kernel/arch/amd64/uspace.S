/* Kiwi AMD64 userspace entry function
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
 * @brief		AMD64 userspace entry function.
 */

#include <arch/x86/sysreg.h>
#include <arch/descriptor.h>
#include <arch/stack.h>

.global uspace_entry
uspace_entry:
	/* Push a zero return address so the userspace program will not fault
	 * if it inadvertently returns from its entry function. */
	subq	$STACK_DELTA, %rsi
	movq	$0, (%rsi)

	/* Set up a stack frame for IRET:
	 *  - SS.
	 *  - RSP.
	 *  - RFLAGS.
	 *  - CS.
	 *  - RIP. */
	push	$(SEG_U_DS | 0x03)
	push	%rsi
	push	$(SYSREG_FLAGS_IF | SYSREG_FLAGS_ALWAYS1)
	push	$(SEG_U_CS | 0x03)
	push	%rdi
	
	/* Set other segment registers. */
	mov	$(SEG_U_DS | 0x03), %ax
	mov	%ax, %ds
	mov	%ax, %es
	
	/* Clear general purpose registers. */
	xorq	%rax, %rax
	xorq	%rbx, %rbx
	xorq	%rcx, %rcx
	xorq	%rdx, %rdx
	xorq	%rsi, %rsi
	xorq	%rdi, %rdi
	xorq	%rbp, %rbp
	xorq	%r8, %r8
	xorq	%r9, %r9
	xorq	%r10, %r10
	xorq	%r11, %r11
	xorq	%r12, %r12
	xorq	%r13, %r13
	xorq	%r14, %r14
	xorq	%r15, %r15

	/* Enter userspace. */
	iretq
.size uspace_entry, .-uspace_entry
