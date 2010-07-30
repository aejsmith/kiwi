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
 * @brief		AMD64 kernel entry points.
 */

#include <arch/asm.h>
#include <arch/cpu.h>
#include <arch/descriptor.h>
#include <arch/stack.h>

#include <status.h>

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
 * @note		Aligned to 16 bytes because the IDT initialisation
 *			code requires each handler to be 16 bytes.
 * @param nr		Interrupt vector number. */
.macro ISR nr
.align 16
	push	$0
	push	$\nr
	jmp	isr_common
.endm

/** Macro to define an ISR with an error code.
 * @note		Aligned to 16 bytes because the IDT initialisation
 *			code requires each handler to be 16 bytes.
 * @param nr		Interrupt vector number. */
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
	mov	%ax, %ss

	/* Clear direction flag. */
	cld

	/* Pass a pointer to the structure we made to the interrupt handler. */
	movq	%rsp, %rdi
	call	intr_handler

	/* Restore the saved registers. */
	POP_REGS

	/* Get rid of the error code and interrupt number. */
	addq	$16, %rsp
	iretq
FUNCTION_END(isr_common)

/** SYSCALL entry point. */
FUNCTION_START(syscall_entry)
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

	/* Save the arguments while we do kernel entry work and get the system
	 * call details. */
	push	%r9
	push	%r8
	push	%r10
	push	%rdx
	push	%rsi
	push	%rdi
	push	%rax

	/* Perform kernel entry work. */
	call	thread_at_kernel_entry

	/* Get back the call number and check whether it is valid. */
	pop	%rax
	cmp	syscall_table_size, %rax
	jae	.Linval

	/* Check the argument count. If there are more than 6 arguments, we
	 * must copy from the stack. */
	shl	$4, %rax
	movq	syscall_table + 8(,%rax,1), %rdx
	cmpq	$6, %rdx
	ja	.Lstackargs

	/* Restore arguments and perform the call. */
	pop	%rdi
	pop	%rsi
	pop	%rdx
	pop	%rcx
	pop	%r8
	pop	%r9
	call	*syscall_table(,%rax,1)
.Lreturn:
	/* Perform kernel exit work. Preserve the system call's return value. */
	push	%rax
	call	thread_at_kernel_exit
	pop	%rax

	/* Clear all non-callee-save registers as we do not want to leak values
	 * used in the kernel to userspace.  */
	xorq	%rdi, %rdi
	xorq	%rsi, %rsi
	xorq	%rdx, %rdx
	xorq	%r10, %r10
	xorq	%r8, %r8
	xorq	%r9, %r9

	/* Restore RFLAGS and RIP for SYSRET to use, as well as the stack
	 * pointer. */
	pop	%r11
	pop	%rcx
	pop	%rsp

	/* Return to user-mode. */
	sysretq
.Lstackargs:
	/* Work out how many bytes to copy, save the call table offset and
	 * current stack pointer, then work out where to copy to. */
	sub	$6, %rdx
	shl	$3, %rdx
	push	%rax
	push	%rbx
	movq	%rsp, %rbx
	sub	%rdx, %rsp

	/* Copy the arguments. The source is the userspace stack pointer + 8. */
	movq	%rsp, %rdi
	movq	80(%rbx), %rsi
	add	$8, %rsi
	call	memcpy_from_user
	cmp	$STATUS_SUCCESS, %eax
	jne	.Lcpyfail

	/* Restore the arguments passed in registers and perform the call. */
	movq	16(%rbx), %rdi
	movq	24(%rbx), %rsi
	movq	32(%rbx), %rdx
	movq	40(%rbx), %rcx
	movq	48(%rbx), %r8
	movq	56(%rbx), %r9
	movq	8(%rbx), %rax
	call	*syscall_table(,%rax,1)

	/* Restore stack pointer and old value of RBX then return. */
	movq	%rbx, %rsp
	pop	%rbx
	add	$56, %rsp
	jmp	.Lreturn
.Lcpyfail:
	movq	$STATUS_ADDR_INVAL, %rax
	movq	%rbx, %rsp
	pop	%rbx
	add	$56, %rsp
	jmp	.Lreturn
.Linval:
	movq	$STATUS_SYSCALL_INVAL, %rax
	add	$48, %rsp
	jmp	.Lreturn
FUNCTION_END(syscall_entry)

/** Userspace entry function. */
FUNCTION_START(thread_arch_enter_userspace)
	/* Push a zero return address so the userspace program will fault if it
	 * inadvertently returns from its entry function. */
	subq	$STACK_DELTA, %rsi
	movq	$0, (%rsi)

	/* Set up a stack frame for IRET:
	 *  - SS.
	 *  - RSP.
	 *  - RFLAGS.
	 *  - CS.
	 *  - RIP. */
	push	$(SEGMENT_U_DS | 0x03)
	push	%rsi
	push	$(X86_FLAGS_IF | X86_FLAGS_ALWAYS1)
	push	$(SEGMENT_U_CS | 0x03)
	push	%rdi
	
	/* Set other segment registers. */
	mov	$(SEGMENT_U_DS | 0x03), %ax
	mov	%ax, %ds
	mov	%ax, %es

	/* Move argument to RDI. */
	movq	%rdx, %rdi
	
	/* Clear general purpose registers (except RDI). */
	xorq	%rax, %rax
	xorq	%rbx, %rbx
	xorq	%rcx, %rcx
	xorq	%rdx, %rdx
	xorq	%rsi, %rsi
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
FUNCTION_END(thread_arch_enter_userspace)
