/*
 * Copyright (C) 2010-2011 Alex Smith
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
