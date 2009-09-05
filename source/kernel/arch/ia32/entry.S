/* Kiwi IA32 kernel entry points
 * Copyright (C) 2008-2009 Alex Smith
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
 * @brief		IA32 kernel entry points.
 */

#include <arch/descriptor.h>
#include <arch/x86/sysreg.h>
#include <arch/stack.h>

/** Push registers onto the stack. */
.macro PUSH_REGS
	pusha

	push	%ds
	andl	$0xFFFF, (%esp)
	push	%es
	andl	$0xFFFF, (%esp)
	push	%fs
	andl	$0xFFFF, (%esp)
	push	%gs
	andl	$0xFFFF, (%esp)
.endm

/** Pop registers from the stack. */
.macro POP_REGS
	pop	%gs
	pop	%fs
	pop	%es
	pop	%ds

	popa
.endm

/** Macro to define an ISR.
 *
 * Defines an ISR handler which pushes the interrupt number and a fake error
 * code to the stack. Aligned to 16 bytes because the IDT initialization
 * code wants each handler to be 16 bytes in size.
 *
 * @param nr		Interrupt vector number.
 */
.macro ISR nr
.align 16
	push	$0
	push	$\nr
	jmp	__isr_common
.endm

/** Macro to define an ISR (with error code).
 *
 * Defines an ISR handler which pushes the interrupt numberto the stack. For
 * interrupts defined using this macro, the CPU should have pushed an error
 * code to the stack. Aligned to 16 bytes because the IDT initialization
 * code wants each handler to be 16 bytes in size.
 *
 * @param nr		Interrupt vector number.
 */
.macro ISR_E nr
.align 16
	push	$\nr
	jmp	__isr_common
.endm

/** Array of ISR handlers, each 16 bytes long. */
.align 16
.global __isr_array
.type __isr_array, @function
__isr_array:
	/* Define the exceptions (0-19) and the reserved interrupts (20-31). */
	ISR	0
	ISR	1
	ISR	2
	ISR	3
	ISR	4
	ISR	5
	ISR	6
	ISR	7
	ISR_E	8
	ISR	9
	ISR_E	10
	ISR_E	11
	ISR_E	12
	ISR_E	13
	ISR_E	14
	ISR	15
	ISR	16
	ISR_E	17
	ISR	18
	ISR	19
	ISR	20
	ISR	21
	ISR	22
	ISR	23
	ISR	24
	ISR	25
	ISR	26
	ISR	27
	ISR	28
	ISR	29
	ISR	30
	ISR	31

	/* Define the user-defined ISRs (32-255) - none take an error code. */
	.Lintr = 32
	.rept 224
		ISR .Lintr
		.Lintr = .Lintr+1
	.endr
.size __isr_array, .-__isr_array

/** Common ISR handling code. */
.type __isr_common, @function
__isr_common:
	/* Create the interrupt frame structure on the stack. */
	PUSH_REGS

	/* Set sane segment descriptors. */
	mov	$SEG_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/* Clear direction flag. */
	cld

	/* Pass the interrupt number and a pointer to the structure we made
	 * to the interrupt handler. Offset for interrupt number is 48, plus 4
	 * to account for the pointer we pushed. */
	push	%esp
	push	52(%esp)
	call	intr_handler
	addl	$8, %esp

	/* Restore the saved registers. */
	POP_REGS

	/* Get rid of the error code and interrupt number. */
	addl	$8, %esp
	iret
.size __isr_common, .-__isr_common

/** Userspace entry function. */
.global thread_arch_enter_userspace
thread_arch_enter_userspace:
	/* Get arguments. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	movl	12(%esp), %eax

	/* Push argument. */
	subl	$STACK_DELTA, %esi
	movl	%eax, (%esi)

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
.size thread_arch_enter_userspace, .-thread_arch_enter_userspace
