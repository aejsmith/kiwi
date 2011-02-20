/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		IA32 kernel entry points.
 */

#include <arch/intr.h>
#include <arch/memory.h>
#include <arch/thread.h>

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>

#include <status.h>

/** Push registers onto the stack and set up a sane state. */
.macro ENTRY_COMMON
	pusha
	push	%ds
	andl	$0xFFFF, (%esp)
	push	%es
	andl	$0xFFFF, (%esp)
	push	%fs
	andl	$0xFFFF, (%esp)
	push	%gs
	andl	$0xFFFF, (%esp)

	/* Set sane segment descriptors. */
	mov	$SEGMENT_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	$SEGMENT_K_GS, %ax
	mov	%ax, %gs

	/* Clear direction flag. */
	cld
.endm

/** Pop registers from the stack. */
.macro RETURN_COMMON
	pop	%gs
	pop	%fs
	pop	%es
	pop	%ds
	popa

	/* Get rid of the error code and interrupt number, and return. */
	addl	$8, %esp
	iret
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
	/* Save previous state and set up a sane state. */
	ENTRY_COMMON

	/* Call the interrupt handler. */
	push	%esp
	call	intr_handler
	add	$4, %esp

	/* Restore the saved state and return. */
	RETURN_COMMON
FUNCTION_END(isr_common)

/** System call interrupt entry point. */
FUNCTION_START(syscall_entry)
	/* Push an interrupt number and error code. */
	push	$0
	push	$SYSCALL_INT_NO

	/* Save previous state and set up a sane state. */
	ENTRY_COMMON

	/* Save the user-mode interrupt frame pointer. */
	movl	%esp, %gs:THREAD_ARCH_OFF_USER_IFRAME

	/* Perform kernel entry work and enable interrupts. */
	call	thread_at_kernel_entry
	sti

	/* Get back the call number and check whether it is valid. */
	movl	IFRAME_OFF_AX(%esp), %edi
	cmp	syscall_table_size, %edi
	jae	.Linval

	/* Check the argument count. If there are more than 6 arguments, we
	 * must copy from the stack. */
	shl	$3, %edi
	movl	syscall_table + 4(,%edi,1), %ecx
	cmpl	$6, %ecx
	ja	.Lstackargs

	/* Restore the arguments passed in registers and perform the call. */
	sub	$24, %esp
	movl	24 + IFRAME_OFF_DX(%esp), %eax
	movl	%eax, 0(%esp)
	movl	24 + IFRAME_OFF_CX(%esp), %eax
	movl	%eax, 4(%esp)
	movl	24 + IFRAME_OFF_DI(%esp), %eax
	movl	%eax, 8(%esp)
	movl	24 + IFRAME_OFF_SI(%esp), %eax
	movl	%eax, 12(%esp)
	movl	24 + IFRAME_OFF_BX(%esp), %eax
	movl	%eax, 16(%esp)
	movl	24 + IFRAME_OFF_BP(%esp), %eax
	movl	%eax, 20(%esp)
	call	*syscall_table(,%edi,1)
	add	$24, %esp
.Lreturn:
	/* Save the return value of the system call. */
	movl	%eax, IFRAME_OFF_AX(%esp)

	/* Disable interrupts and perform kernel exit work. */
	cli
	call	thread_at_kernel_exit

	/* Restore the saved state and return. */
	RETURN_COMMON
.Lstackargs:
	/* Work out how many bytes to copy and reserve space on the stack.
	 *  EDI = table offset, ECX = argument count, EBX = saved SP
	 * EDI and EBX are callee-save registers, and so will not be clobbered
	 * by any called functions. */
	shl	$2, %ecx
	movl	%esp, %ebx
	sub	%ecx, %esp

	/* Copy the arguments. The source is the userspace stack pointer + 4. */
	movl	%esp, %edx
	push	%ecx
	movl	IFRAME_OFF_SP(%ebx), %eax
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
	jmp	.Lreturn
FUNCTION_END(syscall_entry)

/** Userspace entry function. */
FUNCTION_START(ia32_enter_userspace)
	/* Get arguments. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi

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
FUNCTION_END(ia32_enter_userspace)
