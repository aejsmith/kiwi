/* Kiwi AMD64 AP boostrap code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		AMD64 AP boostrap code.
 */

#include <arch/x86/sysreg.h>

.code16

.global _start
.type _start, @function
_start:
	jmp	__ap_boot
.size _start, .-_start

.align 4

/** Where the kernel stores the kernel entry address. */
entry_addr:	.long 0

.type __ap_boot, @function
__ap_boot:
	xorl	%edx, %edx

	/* Set the data segment and save the load location. */
	mov	%cs, %dx
	mov	%dx, %ds
	shl	$4, %edx

	/* Load the GDT. */
	leal	__gdt(%edx), %eax
	movl	%eax, (__gdtp + 2)
	lgdtl	(__gdtp)

	/* Enable protected mode. */
	movl	%cr0, %eax
	orl	$SYSREG_CR0_PE, %eax
	movl	%eax, %cr0

	/* Fix the jump address. */
	leal	.Lpmode(%edx), %eax
	movl	%eax, (1f + 2)
1:	ljmpl	$0x08, $0
.Lpmode:
.code32
	/* Load data segment. */
	movl	$0x10, %eax
	mov	%ax, %ds

	/* Get the kernel entry point and jump to it. */
	jmp	*entry_addr(%edx)
.size __ap_boot, .-__ap_boot

.type __gdt, @object
__gdt:
	.quad 0x0000000000000000		/**< NULL descriptor  (0x00). */
	.quad 0x00CF9A000000FFFF		/**< 32-bit code      (0x08). */
	.quad 0x00CF92000000FFFF		/**< 32-bit data      (0x10). */
.L__gdt_end:
.size __gdt, .-__gdt

.type __gdtp, @object
__gdtp:
	.word .L__gdt_end-__gdt-1
	.long 0
.size __gdtp, .-__gdtp
