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

#include <arch/x86/asm.h>
#include <arch/x86/cpu.h>
#include <arch/x86/descriptor.h>
#include <arch/intr.h>
#include <arch/memory.h>

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
.endm

/** Pop registers from the stack. */
.macro POP_REGS
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
	/* If coming from userspace, load the kernel GS segment base. */
	testl	$3, 24(%rsp)
	jz	1f
	swapgs
1:
	/* Create the interrupt frame structure on the stack. */
	PUSH_REGS

	/* Clear direction flag. */
	cld

	/* Pass a pointer to the structure we made to the interrupt handler. */
	movq	%rsp, %rdi
	call	intr_handler

	/* Restore the saved registers. */
	POP_REGS

	/* If returning to userspace, swap back to the previous GS base. */
	testl	$3, 24(%rsp)
	jz	2f
	swapgs
2:
	/* Get rid of the error code and interrupt number. */
	addq	$16, %rsp
	iretq
FUNCTION_END(isr_common)

/** SYSCALL entry point. */
FUNCTION_START(syscall_entry)
	/* Swap to the kernel GS base address, which points at the current CPU's
	 * architecture data. The second pointer in this is our kernel stack
	 * pointer. The third pointer is a temporary scratch space for us to
	 * save the userspace stack pointer in before we push it to the stack. */
	swapgs
	movq	%rsp, %gs:16
	movq	%gs:8, %rsp

	/* Get back the userspace stack pointer from the temporary scratch
	 * space and put it onto the kernel stack. */
	push	$(SEGMENT_U_DS | 3)
	push	%gs:16

	/* We now have a stack pointer set up, push information SYSCALL saved
	 * for us (R11 = original RFLAGS, RCX = return IP) and the rest of the
	 * interrupt frame. */
	push	%r11
	push	$(SEGMENT_U_CS | 3)
	push	%rcx
	push	$0
	push	$0
	PUSH_REGS

	/* Perform kernel entry work and enable interrupts. */
	call	thread_at_kernel_entry
	sti

	/* Get back the call number and check whether it is valid. */
	movq	IFRAME_OFF_AX(%rsp), %rbx
	cmp	syscall_table_size, %rbx
	jae	.Linval

	/* Check the argument count. If there are more than 6 arguments, we
	 * must copy from the stack. */
	shl	$4, %rbx
	movq	syscall_table + 8(,%rbx,1), %rdx
	cmpq	$6, %rdx
	ja	.Lstackargs

	/* Restore arguments and perform the call. The argument that should be
	 * in RCX is passed in R10, as RCX is used by SYSCALL/SYSRET as the
	 * return IP. */
	movq	IFRAME_OFF_DI(%rsp), %rdi
	movq	IFRAME_OFF_SI(%rsp), %rsi
	movq	IFRAME_OFF_DX(%rsp), %rdx
	movq	IFRAME_OFF_R10(%rsp), %rcx
	movq	IFRAME_OFF_R8(%rsp), %r8
	movq	IFRAME_OFF_R9(%rsp), %r9
	call	*syscall_table(,%rbx,1)
.Lreturn:
	/* Save the return value of the system call. */
	movq	%rax, IFRAME_OFF_AX(%rsp)

	/* Clear interrupts. */
	cli

	/* Perform kernel exit work. This requires an interrupt frame for
	 * signal delivery. If a signal is to be delivered, return via IRET as
	 * we need RCX to be preserved. */
	//movq	%rsp, %rdi
	call	thread_at_kernel_exit
	//test	%rax, %rax
	//jnz	.Liret

	/* Restore saved registers, RFLAGS/RIP for SYSRET, and the userspace
	 * stack pointer. */
	POP_REGS
	add	$16, %rsp
	pop	%rcx
	add	$8, %rsp
	pop	%r11
	pop	%rsp

	/* Restore previous GS base and return to user-mode. */
	swapgs
	sysretq
.Lstackargs:
	/* Work out how many bytes to copy and reserve space on the stack.
	 *  RDX = argument count, RBX = call table offset, R12 = saved SP.
	 * RDX is in the correct location to pass to memcpy_from_user() after
	 * modifying, RBX and R12 are callee-save so they won't be clobbered by
	 * any called functions. */
	sub	$6, %rdx
	shl	$3, %rdx
	movq	%rsp, %r12
	sub	%rdx, %rsp

	/* Copy the arguments. The source is the userspace stack pointer + 8. */
	movq	%rsp, %rdi
	movq	IFRAME_OFF_SP(%r12), %rsi
	add	$8, %rsi
	call	memcpy_from_user
	cmp	$STATUS_SUCCESS, %eax
	jne	.Lcpyfail

	/* Restore the arguments passed in registers and perform the call. */
	movq	IFRAME_OFF_DI(%r12), %rdi
	movq	IFRAME_OFF_SI(%r12), %rsi
	movq	IFRAME_OFF_DX(%r12), %rdx
	movq	IFRAME_OFF_R10(%r12), %rcx
	movq	IFRAME_OFF_R8(%r12), %r8
	movq	IFRAME_OFF_R9(%r12), %r9
	call	*syscall_table(,%rbx,1)

	/* Restore stack pointer then return. */
	movq	%r12, %rsp
	jmp	.Lreturn
.Liret:
	/* Restore the registers, get rid of interrupt/error code. */
	POP_REGS
	addq	$16, %rsp

	/* Return to userspace. */
	swapgs
	iretq
.Lcpyfail:
	movq	$STATUS_INVALID_ADDR, %rax
	movq	%r12, %rsp
	jmp	.Lreturn
.Linval:
	movq	$STATUS_INVALID_SYSCALL, %rax
	jmp	.Lreturn
FUNCTION_END(syscall_entry)

/** Userspace entry function. */
FUNCTION_START(thread_arch_enter_userspace)
	/* Push a zero return address so the userspace program will fault if it
	 * inadvertently returns from its entry function. */
	subq	$8, %rsi
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
	swapgs
	iretq
FUNCTION_END(thread_arch_enter_userspace)
