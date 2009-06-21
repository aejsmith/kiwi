/* Kiwi AMD64 kernel entry points
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
 * @brief		AMD64 kernel entry points.
 */

#include <arch/descriptor.h>

/** Push registers onto the stack. */
.macro PUSH_REGS
	push	%rax
	push	%rbx
	push	%rcx
	push	%rdx
	push	%rdi
	push	%rsi
	push	%rbp
	push	%r8
	push	%r9
	push	%r10
	push	%r11
	push	%r12
	push	%r13
	push	%r14
	push	%r15

	push	%fs
	andl	$0xFFFF, (%rsp)
	push	%gs
	andl	$0xFFFF, (%rsp)
.endm

/** Pop registers from the stack. */
.macro POP_REGS
	pop	%gs
	pop	%fs
	pop	%r15
	pop	%r14
	pop	%r13
	pop	%r12
	pop	%r11
	pop	%r10
	pop	%r9
	pop	%r8
	pop	%rbp
	pop	%rsi
	pop	%rdi
	pop	%rdx
	pop	%rcx
	pop	%rbx
	pop	%rax
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
	mov	%ax, %ss

	/* Clear direction flag. */
	cld

	/* Pass the interrupt number and a pointer to the structure we made
	 * to the interrupt handler. */
	movq	136(%rsp), %rdi
	movq	%rsp, %rsi
	call	intr_handler

	/* Restore the saved registers. */
	POP_REGS

	/* Get rid of the error code and interrupt number. */
	addq	$16, %rsp
	iretq
.size __isr_common, .-__isr_common

/** SYSCALL entry point. */
.global __syscall_entry
.type __syscall_entry, @function
__syscall_entry:
	/* Switch to the kernel mode stack. The GS_BASE MSR points to the
	 * thread's architecture-specific data. Use the SWAPGS instruction
	 * to get at it, so we can get our stack pointer. The userspace RSP
	 * field is not used in returning - it is merely a temporary scratch
	 * space we use before we push it onto the stack for the system call
	 * frame. */
	swapgs
	movq	%rsp, %gs:8
	movq	%gs:0, %rsp

	/* Get back the userspace stack pointer from the temporary scratch
	 * space and put it onto the kernel stack (at the top of the system
	 * call frame). */
	push	%gs:8

	/* Return to the previous GS value. */
	swapgs

	/* We now have a stack pointer set up, push information SYSCALL saved
	 * for us (RAX = return IP, R11 = original RFLAGS). */
	push	%rcx
	push	%r11

	/* Push the callee-save registers. We must push callee-save registers
	 * despite the fact that they are not modified by this function, as it
	 * must be possible to duplicate a thread using a system call frame,
	 * i.e. to implement fork() in the POSIX subsystem. */
	push	%r12
	push	%r13
	push	%r14
	push	%r15
	push	%rbx
	push	%rbp

	/* Push arguments. R10 is used over RCX for the fourth argument, as
	 * RCX is used by SYSCALL to store the return address. */
	push	%r8
	push	%r10
	push	%rdx
	push	%rsi
	push	%rdi

	/* Push system call ID. */
	push	%rax

	/* Now that our stack frame is set up we can enable interrupts - they
	 * do not need to be off across a system call. */
	sti

	/* Store a pointer to the system call frame as the first argument
	 * and call the system call dispatcher. */
	movq	%rsp, %rdi
	call	syscall_handler

	/* Disable interrupts and restore saved state. Do not need to restore
	 * callee-save registers as they are preserved across the call to
	 * syscall_handler() - see comment above about why they are saved in
	 * the first place. */
	cli
	addq	$96, %rsp
	pop	%r11
	pop	%rcx
	pop	%rsp

	/* Return to user-mode. */
	sysretq
.size __syscall_entry, .-__syscall_entry
