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

/** Real-mode startup function. */
.global _start
.type _start, @function
_start:
.code16
	/* Don't want interrupts. */
	cli

	/* Set up data segments. */
	movw	$(LOADER_LOAD_ADDR >> 4), %ax
	movw	%ax, %ds
	movw	%ax, %es

	/* Save the boot device/partition offset. */
	addr32 movb	%dl, (boot_device_id - LOADER_LOAD_ADDR)
	addr32 movl	%ebx, (boot_part_offset - LOADER_LOAD_ADDR)
	addr32 movl	%ecx, (boot_part_offset - LOADER_LOAD_ADDR) + 4

	/* Make sure we're displaying page 0, and move the cursor to (0,0) */
	movb	$0x02, %ah
	xorb	%bh, %bh
	xorw	%dx, %dx
	int	$0x10

	/* Enable the A20 line. */
	call	enable_a20

	/* Now we need to switch to protected mode. */
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

	/* Jump to the common startup code. */
	jmp	__loader_entry
.size _start, .-_start

/** Test whether the A20 line is enabled.
 * @note		Partially based on code found at
 *			http://wiki.osdev.org/A20_Line
 * @return		AX = 1 if enabled, AX = 0 if not. */
.type test_a20, @function
test_a20:
.code16
	/* Iterate checking to give time for enabling attempts we make to take
	 * effect. */
	push	%cx
	mov	$0x1000, %cx

	/* Set segment registers and addresses to compare. */
1:	xor	%ax, %ax
	mov	%ax, %fs
	not	%ax
	mov	%ax, %gs
	mov	$0x1000, %di
	mov	$0x1010, %si

	/* Write different values to the two addresses, and compare. If they
	 * both equal the second value written, the A20 line is not enabled. */
	movb	$0x00, %fs:(%di)
	outb	%al, $0x80
	movb	$0xFF, %gs:(%si)
	outb	%al, $0x80
	cmpb	$0xFF, %fs:(%di)
	jne	2f
	loop	1b
	xor	%ax, %ax
	pop	%cx
	ret
2:	mov	$1, %ax
	pop	%cx
	ret
.size test_a20, .-test_a20

/** Enable the A20 line. */
.type enable_a20, @function
enable_a20:
	mov	$0x100, %cx
.Lbegin:
	/* Don't do anything if already enabled. */
	call	test_a20
	cmp	$0, %ax
	je	.Lenable_bios
	ret
.Lenable_bios:
	/* First try using a BIOS interrupt. */
	mov	$0x2401, %ax
	int	$0x15
	call	test_a20
	cmp	$0, %eax
	je	.Lenable_kbd
	ret
.Lenable_kbd:
	/* Try the keyboard controller. */
	call	.Lflush
	mov	$0xD1, %al
	outb	%al, $0x64
	call	.Lflush
	mov	$0xDF, %al
	outb	%al, $0x60
	call	.Lflush
	mov	$0xFF, %al
	outb	%al, $0x64
	call	.Lflush
	call	test_a20
	cmp	$0, %eax
	je	.Lenable_fast
	ret
.Lenable_fast:
	/* Try the fast A20 mechanism. */
	inb	$0x92, %al
	testb	$(1<<1), %al
	jnz	.Lfail
	orb	$(1<<1), %al
	andb	$~(1<<0), %al
	outb	%al, $0x92
	call	test_a20
	cmp	$0, %eax
	je	.Lfail
	ret
.Lfail:
	loop	.Lbegin
	mov	$'a', %al
	mov	$0x3F8, %dx
	outb	%al, (%dx)
1:	jmp	1b
.Lflush:
	outb	%al, $0x80
	in	$0x64, %al
	test	$1, %al
	jz	2f
	outb	%al, $0x80
	in	$0x60, %al
	jmp	.Lflush
2:	test	$2, %al
	jnz	.Lflush
	ret
.size enable_a20, .-enable_a20

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
.code32
	/* Don't want interrupts. */
	cli

	/* Relocate the loader image. */
	cld
	mov	$(__end - LOADER_LOAD_ADDR), %ecx
	mov	$LOADER_LOAD_ADDR, %edi
	mov	$MULTIBOOT_LOAD_ADDR, %esi
	rep movsb

	/* Load the GDT. */
	lgdt	(loader_gdtp)
	ljmp	$SEGMENT_CS, $1f
1:
	mov	$SEGMENT_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Save information pointer. */
	movl	%ebx, (multiboot_info)

	/* Jump to the common startup code. */
	jmp	__loader_entry
.size __multiboot_entry, .-__multiboot_entry

/** Common startup function. */
.type __loader_entry, @function
__loader_entry:
	/* Set the stack pointer to the physical location of our stack and
	 * clear the stack frame/EFLAGS. */
	movl	$boot_stack + KSTACK_SIZE, %esp
	xorl	%ebp, %ebp
	push	$0
	popf

	/* Call the main function. */
	call	loader_main
1:	jmp	1b
.size __loader_entry, .-__loader_entry

/** GDT pointer. */
.global loader_gdtp
.type loader_gdtp, @object
loader_gdtp:
        .word .L__gdt_end-__gdt-1
        .long __gdt
.size loader_gdtp, .-loader_gdtp

/** Boot drive. */
.global boot_device_id
.type boot_device_id, @object
boot_device_id:
	.byte 0
.size boot_device_id, .-boot_device_id

/** Boot partition offset. */
.global boot_part_offset
.type boot_part_offset, @object
boot_part_offset:
	.quad 0
.size boot_part_offset, .-boot_part_offset

.section .data, "aw", @progbits

/** Stack. */
.align PAGE_SIZE
.global boot_stack
.type boot_stack, @object
boot_stack:
	.fill	KSTACK_SIZE
.size boot_stack, .-boot_stack

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
.global multiboot_info
.type multiboot_info, @object
multiboot_info:
	.long 0
.size multiboot_info, .-multiboot_info
