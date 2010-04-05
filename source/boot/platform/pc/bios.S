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
 * @brief		BIOS interrupt functions.
 */

#include <arch/asm.h>
#include <platform/boot.h>

/** Size of the registers structure. */
#define REGS_SIZE	0x24

/** Stack to use in real mode. */
#define REAL_MODE_STACK	0xFFFC
#define REAL_MODE_SP	(REAL_MODE_STACK - REGS_SIZE)

.section .text, "ax", @progbits

/** Call a BIOS interrupt.
 * @param num		Interrupt number.
 * @param regs		Registers to use for the interrupt. */
FUNCTION_START(bios_interrupt)
	/* Save callee-save registers/flags. */
	push	%ebp
	push	%ebx
	push	%edi
	push	%esi
	pushf

	/* Write the interrupt number. */
	movl	24(%esp), %eax
	movb	%al, .Lint

	/* Copy the registers structure to the real mode stack. */
	movl	28(%esp), %esi
	movl	$REAL_MODE_SP, %edi
	movl	$REGS_SIZE, %ecx
	rep movsb

	/* Save the stack pointer at the top of the real mode stack and switch
	 * to it. */
	movl	%esp, (REAL_MODE_STACK)
	movl	$REAL_MODE_SP, %esp

	/* Switch to the BIOS IVT. */
	lidt	(bios_idtp)

	/* Jump into the real mode code segment. */
	ljmp	$SEGMENT_CS16, $1f - LOADER_LOAD_ADDR

	/* Clear the PE (Protection Enable) bit of CR0. */
1:	movl	%cr0, %eax
	andl	$~(1<<0), %eax
	movl	%eax, %cr0

	/* Set correct segments. */
	.byte 0x66
	ljmp	$(LOADER_LOAD_ADDR >> 4), $2f - LOADER_LOAD_ADDR
.code16
2:	mov	$0, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Get the registers to use. */
	pop	%eax
	movl	28(%esp), %eax
	mov	%ax, %es
	movl	0(%esp), %eax
	movl	4(%esp), %ebx
	movl	8(%esp), %ecx
	movl	12(%esp), %edx
	movl	16(%esp), %edi
	movl	20(%esp), %esi
	movl	24(%esp), %ebp

	/* Re-enable interrupts. */
	sti

	/* Hard-code in an INT instruction that gets modified above to contain
	 * the correct interrupt number. */
	.byte 0xCD
.Lint:	.byte 0x0

	/* Save the new register/flags state. */
	mov	%eax, 0(%esp)
	mov	%ebx, 4(%esp)
	mov	%ecx, 8(%esp)
	mov	%edx, 12(%esp)
	mov	%edi, 16(%esp)
	mov	%esi, 20(%esp)
	mov	%ebp, 24(%esp)
	pushfl

	/* Disable interrupts again. */
	cli

	/* Return to protected mode. */
	movl	%cr0, %eax
	orl	$(1<<0), %eax
	movl	%eax, %cr0
	data32 ljmp $SEGMENT_CS, $3f
.code32
3:
	mov	$SEGMENT_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Switch back to the loader IDT. */
	lidt	(loader_idtp)

	/* Restore stack pointer. */
	mov	(REAL_MODE_STACK), %esp

	/* Copy the registers structure back from the real mode stack. */
	movl	$REAL_MODE_SP, %esi
	movl	28(%esp), %edi
	movl	$REGS_SIZE, %ecx
	rep movsb

	/* Pop callee-save registers/flags and return. */
	popf
	pop	%esi
	pop	%edi
	pop	%ebx
	pop	%ebp
	ret
FUNCTION_END(bios_interrupt)

/** IDT pointer for the BIOS IVT. */
bios_idtp:
        .word 0x7FF
        .long 0
