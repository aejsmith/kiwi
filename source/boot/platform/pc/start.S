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
 * @brief		x86 bootloader startup code.
 */

#include <arch/page.h>
#include <arch/stack.h>

#include <platform/boot.h>

#include "multiboot.h"

/* Ensure that this code is linked in first. */
.section .init.text, "ax", @progbits
.code16

/** Real-mode startup function. */
.global _start
.type _start, @function
_start:
	/* Don't want interrupts. */
	cli

	/* Set up data segments. */
	movw	$(LOADER_LOAD_ADDR >> 4), %ax
	movw	%ax, %ds
	movw	%ax, %es

	/* Save the boot device/partition offset. */
	addr32 movb	%dl, (g_boot_device - LOADER_LOAD_ADDR)
	addr32 movl	%ebx, (g_boot_offset - LOADER_LOAD_ADDR)
	addr32 movl	%ecx, (g_boot_offset - LOADER_LOAD_ADDR) + 4

	/* Make sure we're displaying page 0, and move the cursor to (0,0) */
	movb	$0x02, %ah
	xorb	%bh, %bh
	xorw	%dx, %dx
	int	$0x10

	/* Enable the A20 line. I'm using the fast A20 mechanism here - the
	 * Haiku bootloader uses it as the only method of enabling it, and
	 * nobody has complained that it doesn't work. */
	inb	$0x92, %al
	testb	$0x02, %al
	jnz	1f
	orb	$(1<<1), %al
	andb	$~(1<<0), %al
	outb	%al, $0x92

	/* Now we need to switch to protected mode. */
1:	addr32	lgdt	(g_gdt_pointer - LOADER_LOAD_ADDR)
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

	/* Jump to the common startup code. */
	jmp	__loader_entry
.size _start, .-_start

/** Multiboot header structure. */
.align 4
.type multiboot_header, @object
multiboot_header:
	.long MB_HEADER_MAGIC
	.long (MB_HFLAG_KLUDGE | MB_HFLAG_MODALIGN)
	.long -(MB_HEADER_MAGIC + (MB_HFLAG_KLUDGE | MB_HFLAG_MODALIGN))
	.long multiboot_header + MULTIBOOT_LOAD_OFFSET
	.long __text_start + MULTIBOOT_LOAD_OFFSET
	.long __data_end + MULTIBOOT_LOAD_OFFSET
	.long __end + MULTIBOOT_LOAD_OFFSET
	.long __multiboot_entry + MULTIBOOT_LOAD_OFFSET
.size multiboot_header, .-multiboot_header

/** Multiboot startup function. */
.type __multiboot_entry, @function
__multiboot_entry:
	/* Don't want interrupts. */
	cli

	/* Relocate the loader image. */
	cld
	mov	$(__end - LOADER_LOAD_ADDR), %ecx
	mov	$LOADER_LOAD_ADDR, %edi
	mov	$MULTIBOOT_LOAD_ADDR, %esi
	rep movsb

	/* Load the GDT. */
	lgdt	(g_gdt_pointer)
	ljmp	$SEGMENT_CS, $1f
1:
	mov	$SEGMENT_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Save information pointer. */
	movl	%ebx, (g_multiboot_info)

	/* Jump to the common startup code. */
	jmp	__loader_entry
.size __multiboot_entry, .-__multiboot_entry

/** Common startup function. */
.type __loader_entry, @function
__loader_entry:
	/* Set the stack pointer to the physical location of our stack and
	 * clear the stack frame/EFLAGS. */
	movl	$g_boot_stack + KSTACK_SIZE, %esp
	xorl	%ebp, %ebp
	push	$0
	popf

	/* Call the main function. */
	call	loader_main
1:	jmp	1b
.size __loader_entry, .-__loader_entry

/** GDT pointer. */
.global g_gdt_pointer
.type g_gdt_pointer, @object
g_gdt_pointer:
        .word .L__gdt_end-__gdt-1
        .long __gdt
.size g_gdt_pointer, .-g_gdt_pointer

/** Boot drive. */
.global g_boot_device
.type g_boot_device, @object
g_boot_device:
	.byte 0
.size g_boot_device, .-g_boot_device

/** Boot partition offset. */
.global g_boot_offset
.type g_boot_offset, @object
g_boot_offset:
	.quad 0
.size g_boot_offset, .-g_boot_offset

.section .data, "aw", @progbits

/** Stack. */
.align PAGE_SIZE
.global g_boot_stack
.type g_boot_stack, @object
g_boot_stack:
	.fill	KSTACK_SIZE
.size g_boot_stack, .-g_boot_stack

/** Global descriptor table. */
.type __gdt, @object
__gdt:
	.quad 0x0000000000000000                /**< NULL descriptor (0x00). */
        .quad 0x00CF9A000000FFFF                /**< 32-bit code     (0x08). */
        .quad 0x00CF92000000FFFF                /**< 32-bit data     (0x10). */
	.quad 0x00009A010000FFFF		/**< 16-bit code     (0x18). */
	.quad 0x00AF9A000000FFFF		/**< 64-bit code     (0x20). */
	.quad 0x008F92000000FFFF		/**< 64-bit data     (0x28). */
.L__gdt_end:
.size __gdt, .-__gdt

/** Multiboot information pointer. */
.global g_multiboot_info
.type g_multiboot_info, @object
g_multiboot_info:
	.long 0
.size g_multiboot_info, .-g_multiboot_info
