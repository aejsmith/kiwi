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
 * @brief		PC kernel entry functions.
 */

#include <arch/x86/asm.h>
#include <platform/boot.h>

#define CHAINLOAD_STACK		0x2000
#define CHAINLOAD_ADDR		0x7C00

.section .text, "ax", @progbits

/** Drop to real mode and jump to a new boot sector.
 * @param id		ID of boot disk.
 * @param part		Address of partition table entry. */
FUNCTION_START(chain_loader_enter)
	/* Save arguments. */
	movl	4(%esp), %edx
	movl	8(%esp), %esi

	cli

	/* Switch to the real-mode stack. */
	movl	$CHAINLOAD_STACK, %esp

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

	sti

	/* Jump to the loader. */
	ljmp	$0, $CHAINLOAD_ADDR
FUNCTION_END(chain_loader_enter)
