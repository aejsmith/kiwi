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
 * @brief		AP bootstrap code.
 */

#include <arch/x86/asm.h>
#include <platform/boot.h>

.section .trampoline, "ax", @progbits
.code16

/** AP trampoline code. */
FUNCTION_START(ap_trampoline)
	/* Don't want interrupts. */
	cli

	/* Set up data segments. */
	movw	$(LOADER_LOAD_ADDR >> 4), %ax
	movw	%ax, %ds
	movw	%ax, %es

	/* Switch to protected mode. */
1:	addr32	lgdt	(loader_gdtp - LOADER_LOAD_ADDR)
	movl	%cr0, %eax
	orl	$(1<<0), %eax
	movl	%eax, %cr0
	data32 ljmp $SEGMENT_CS, $2f
.code32
2:
	mov	$SEGMENT_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Set the stack pointer. */
	movl	ap_stack_ptr, %esp

	/* Load the bootloader IDT. */
	lidt	(loader_idtp)

	/* Jump to the kernel. */
	jmp	cpu_ap_entry
FUNCTION_END(ap_trampoline)
