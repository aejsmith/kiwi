/* Kiwi IA32 userspace entry function
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
 * @brief		IA32 userspace entry function.
 */

#include <arch/x86/sysreg.h>
#include <arch/descriptor.h>
#include <arch/stack.h>

.global uspace_entry
uspace_entry:
	/* Get arguments. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi

	/* Push a zero return address so the userspace program will not fault
	 * if it inadvertently returns from its entry function. */
	subl	$STACK_DELTA, %esi
	movl	$0, (%esi)

	/* Set up a stack frame for IRET:
	 *  - SS.
	 *  - ESP.
	 *  - EFLAGS.
	 *  - CS.
	 *  - EIP. */
	push	$(SEG_U_DS | 0x03)
	push	%esi
	push	$(SYSREG_FLAGS_IF | SYSREG_FLAGS_ALWAYS1)
	push	$(SEG_U_CS | 0x03)
	push	%edi
	
	/* Set other segment registers. */
	mov	$(SEG_U_DS | 0x03), %ax
	mov	%ax, %ds
	mov	%ax, %es
	
	/* Clear general purpose registers. */
	xorl	%eax, %eax
	xorl	%ebx, %ebx
	xorl	%ecx, %ecx
	xorl	%edx, %edx
	xorl	%esi, %esi
	xorl	%edi, %edi
	xorl	%ebp, %ebp

	/* Enter userspace. */
	iret
.size uspace_entry, .-uspace_entry
