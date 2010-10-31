/*
 * Copyright (C) 2008-2010 Alex Smith
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

#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/descriptor.h>
#include <arch/stack.h>

#include <status.h>

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
 * code to the stack. Aligned to 16 bytes because the IDT initialisation
 * code wants each handler to be 16 bytes in size.
 *
 * @param nr		Interrupt vector number.
 */
.macro ISR nr
.align 16
	push	$0
	push	$\nr
	jmp	isr_common
.endm

/** Macro to define an ISR (with error code).
 *
 * Defines an ISR handler which pushes the interrupt numberto the stack. For
 * interrupts defined using this macro, the CPU should have pushed an error
 * code to the stack. Aligned to 16 bytes because the IDT initialisation
 * code wants each handler to be 16 bytes in size.
 *
 * @param nr		Interrupt vector number.
 */
.macro ISR_E nr
.align 16
	push	$\nr
	jmp	isr_common
.endm

/** Array of ISR handlers, each 16 bytes long. */
.align 16
FUNCTION_START(isr_array)
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
FUNCTION_END(isr_array)

/** Common ISR handling code. */
PRIVATE_FUNCTION_START(isr_common)
	/* Create the interrupt frame structure on the stack. */
	PUSH_REGS

	/* Set sane segment descriptors. */
	mov	$SEGMENT_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

	/* Clear direction flag. */
	cld

	/* Pass a pointer to the structure we made to the interrupt handler. */
	push	%esp
	call	intr_handler
	addl	$4, %esp

	/* Restore the saved registers. */
	POP_REGS

	/* Get rid of the error code and interrupt number. */
	addl	$8, %esp
	iret
FUNCTION_END(isr_common)

/** System call interrupt entry point. */
FUNCTION_START(syscall_entry)
	/* Save current segment descriptors. */
	push	%ds
	andl	$0xFFFF, (%esp)
	push	%es
	andl	$0xFFFF, (%esp)
	push	%fs
	andl	$0xFFFF, (%esp)
	push	%gs
	andl	$0xFFFF, (%esp)

	/* Set sane segment descriptors. */
	push	%eax
	mov	$SEGMENT_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	pop	%eax

	/* Clear direction flag. */
	cld

	/* Save the arguments while we do kernel entry work and get the system
	 * call details. */
	push	%ebp
	push	%ebx
	push	%esi
	push	%edi
	push	%ecx
	push	%edx
	push	%eax

	/* Perform kernel entry work and enable interrupts. */
	call	thread_at_kernel_entry
	sti

	/* Get back the call number and check whether it is valid. */
	pop	%edi
	cmp	syscall_table_size, %edi
	jae	.Linval

	/* Check the argument count. If there are more than 6 arguments, we
	 * must copy from the stack. */
	shl	$3, %edi
	movl	syscall_table + 4(,%edi,1), %esi
	cmpl	$6, %esi
	ja	.Lstackargs

	/* Perform the call. */
	call	*syscall_table(,%edi,1)
	addl	$8, %esp
.Lreturn:
	/* Disable interrupts. */
	cli

	/* At this point the stack pointer should point to the callee-save
	 * registers. Restore them. */
	pop	%edi
	pop	%esi
	pop	%ebx
	pop	%ebp

	/* Perform kernel exit work. Preserve the system call's return value. */
	push	%eax
	call	thread_at_kernel_exit
	pop	%eax

	/* Clear all non-callee-save registers as we do not want to leak values
	 * used in the kernel to userspace.  */
	xorl	%edx, %edx
	xorl	%ecx, %ecx

	/* Restore segment descriptors. */
	pop	%gs
	pop	%fs
	pop	%es
	pop	%ds

	/* Return to user-mode. */
	iret
.Lstackargs:
	/* All registers are passed on the stack. Get rid of the two registers
	 * we won't need to restore. */
	addl	$8, %esp

	/* Work out how many bytes to copy and save the current stack pointer.
	 * EDI = table offset, ESI = argument count. */
	shl	$2, %esi
	movl	%esp, %ebx
	sub	%esi, %esp

	/* Copy the arguments. The source is the userspace stack pointer + 4. */
	movl	%esp, %edx
	push	%esi
	movl	44(%ebx), %eax
	addl	$4, %eax
	push	%eax
	push	%edx
	call	memcpy_from_user
	addl	$12, %esp
	cmp	$STATUS_SUCCESS, %eax
	jne	.Lcpyfail

	/* Perform the call. */
	call	*syscall_table(,%edi,1)

	/* Restore stack pointer and return. */
	movl	%ebx, %esp
	jmp	.Lreturn
.Lcpyfail:
	movl	$STATUS_INVALID_ADDR, %eax
	movl	%ebx, %esp
	jmp	.Lreturn
.Linval:
	movl	$STATUS_INVALID_SYSCALL, %eax
	addl	$8, %esp
	jmp	.Lreturn
FUNCTION_END(syscall_entry)

/** Userspace entry function. */
FUNCTION_START(thread_arch_enter_userspace)
	/* Get arguments. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	movl	12(%esp), %eax

	/* Push argument. */
	subl	$STACK_DELTA, %esi
	movl	%eax, (%esi)

	/* Push a zero return address so the userspace program will fault if it
	 * inadvertently returns from its entry function. */
	subl	$STACK_DELTA, %esi
	movl	$0, (%esi)

	/* Set up a stack frame for IRET:
	 *  - SS.
	 *  - ESP.
	 *  - EFLAGS.
	 *  - CS.
	 *  - EIP. */
	push	$(SEGMENT_U_DS | 0x03)
	push	%esi
	push	$(X86_FLAGS_IF | X86_FLAGS_ALWAYS1)
	push	$(SEGMENT_U_CS | 0x03)
	push	%edi
	
	/* Set other segment registers. */
	mov	$(SEGMENT_U_DS | 0x03), %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	$(SEGMENT_U_GS | 0x03), %ax
	mov	%ax, %gs

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
FUNCTION_END(thread_arch_enter_userspace)
