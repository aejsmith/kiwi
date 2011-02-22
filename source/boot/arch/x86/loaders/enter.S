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
 * @brief		x86 kernel entry functions.
 */

#include <arch/page.h>

#include <x86/asm.h>
#include <x86/cpu.h>

#include <platform/loader.h>

.section .text, "ax", @progbits

/** Enter a 64-bit KBoot kernel.
 * @param tags		Address of tag list.
 * @param cr3		Value to load into CR3.
 * @param entry		Entry point address. */
FUNCTION_START(kboot_arch_enter64)
.code32
	/* Enable PAE. */
	movl	%cr4, %eax
	orl	$X86_CR4_PAE, %eax
	movl	%eax, %cr4

	/* Point CR3 to the boot PML4. */
	movl	12(%esp), %eax
	movl	%eax, %cr3

	/* Enable long mode by setting EFER.LME. */
	movl	$X86_MSR_EFER, %ecx
	rdmsr
	orl	$X86_EFER_LME, %eax
	wrmsr

	/* Set PG (Paging Enable) to put us in compatibility mode. */
	movl	%cr0, %ecx
	orl	$X86_CR0_PG, %ecx
	movl	%ecx, %cr0

	/* Jump into the 64-bit code segment. */
	ljmp	$SEGMENT_CS64, $.Llmode
.align 8
.code64
.Llmode:
	/* Set data segments. */
	mov	$SEGMENT_DS64, %ax
	mov	%ax, %ss
	xorl	%eax, %eax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs

        /* Clear the high 32 bits of %rsp. */
        mov     %esp, %esp

	/* Retreive the tag list address and entry point. */
	movq	4(%rsp), %rdi
	movq	16(%rsp), %rax

	/* Set the stack pointer to the top of the stack. */
	addl	$PAGE_SIZE, %esp
	andl	$0xFFFFF000, %esp

	/* Clear the stack frame/RFLAGS. */
	xorq	%rbp, %rbp
	push	$0
	popf

	/* Call the kernel. */
	call	*%rax
FUNCTION_END(kboot_arch_enter64)

/** Enter a 32-bit KBoot kernel.
 * @param tags		Address of tag list.
 * @param cr3		Value to load into CR3.
 * @param entry		Entry point address. */
FUNCTION_START(kboot_arch_enter32)
.code32
	/* Enable PSE, the MMU code may have created mappings with large pages. */
	movl	%cr4, %eax
	orl	$X86_CR4_PSE, %eax
	movl	%eax, %cr4

	/* Point CR3 to the boot page directory. */
	movl	12(%esp), %eax
	movl	%eax, %cr3

	/* Set PG (Paging Enable). */
	movl	%cr0, %ecx
	orl	$X86_CR0_PG, %ecx
	movl	%ecx, %cr0

	/* Retreive the tag list address and entry point. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	movl	16(%esp), %eax

	/* Set the stack pointer to the top of the stack. */
	addl	$PAGE_SIZE, %esp
	andl	$0xFFFFF000, %esp

	/* Clear the stack frame/EFLAGS. */
	xorl	%ebp, %ebp
	push	$0
	popf

	/* Call the kernel. */
	push	%esi
	push	%edi
	call	*%eax
FUNCTION_END(kboot_arch_enter32)
