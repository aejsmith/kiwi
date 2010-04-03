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
 * @brief		x86 kernel entry functions.
 */

#include <arch/cpu.h>
#include <arch/page.h>
#include <platform/boot.h>

.section .text, "ax", @progbits

/** Enter a 64-bit kernel.
 * @param args		Kernel arguments.
 * @param cpu		CPU ID.
 * @param cr3		Value to load into CR3.
 * @param entry		Entry point address. */
.global arch_enter_kernel64
.type arch_enter_kernel64, @function
arch_enter_kernel64:
.code32
	/* Enable PAE/PGE/OSFXSR. */
	movl	%cr4, %eax
	orl	$(X86_CR4_PAE | X86_CR4_PGE | X86_CR4_OSFXSR), %eax
	movl	%eax, %cr4

	/* Point CR3 to the boot PML4. */
	movl	12(%esp), %eax
	movl	%eax, %cr3

	/* Enable long mode by setting EFER.LME. */
	movl	$X86_MSR_EFER, %ecx
	rdmsr
	orl	$X86_EFER_LME, %eax
	wrmsr

	/* Set PG/WP/NE/MP/TS in CR0 (Paging Enable, Write Protect, Numeric
	 * Error, Monitor Coprocessor, Task Switch), and clear EM (Emulation).
	 * Paging will put us in compatibility mode. TS is set because we do
	 * not want the FPU to be enabled initially. */
	movl	%cr0, %ecx
	orl	$(X86_CR0_PG | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP | X86_CR0_TS), %ecx
	andl	$~(X86_CR0_EM), %ecx
	movl	%ecx, %cr0

	/* Jump into the 64-bit code segment. */
	ljmp	$SEGMENT_CS64, $.Llmode
.align 8
.code64
.Llmode:
	/* Set data segments. */
	mov	$SEGMENT_DS64, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

        /* Clear the high 32 bits of %rsp. */
        mov     %esp, %esp

	/* Retreive the kernel arguments and entry point. */
	movl	4(%rsp), %edi
	movl	8(%rsp), %esi
	movq	16(%rsp), %rax

	/* Set the stack pointer to the top of the stack. */
	addl	$PAGE_SIZE, %esp
	andl	$0xFFFFF000, %esp

	/* Clear the stack frame. */
	xorq	%rbp, %rbp

	/* Call the kernel. */
	call	*%rax
.size arch_enter_kernel64, .-arch_enter_kernel64

/** Enter a 32-bit kernel.
 * @param args		Kernel arguments.
 * @param cpu		CPU ID.
 * @param cr3		Value to load into CR3.
 * @param entry		Entry point address. */
.global arch_enter_kernel32
.type arch_enter_kernel32, @function
arch_enter_kernel32:
.code32
	/* Enable PAE/PGE/OSFXSR. Also enable PSE because although its not
	 * necessary when PAE is being used, VirtualBox shits itself without
	 * it, even though every other x86 box I have doesn't. */
	movl	%cr4, %eax
	orl	$(X86_CR4_PAE | X86_CR4_PGE | X86_CR4_PSE | X86_CR4_OSFXSR), %eax
	movl	%eax, %cr4

	/* Point CR3 to the boot PDP. */
	movl	12(%esp), %eax
	movl	%eax, %cr3

	/* Set PG/WP/NE/MP/TS in CR0 (Paging Enable, Write Protect, Numeric
	 * Error, Monitor Coprocessor, Task Switch), and clear EM (Emulation).
	 * TS is set because we do not want the FPU to be enabled initially. */
	movl	%cr0, %ecx
	orl	$(X86_CR0_PG | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP | X86_CR0_TS), %ecx
	andl	$~(X86_CR0_EM), %ecx
	movl	%ecx, %cr0

	/* Retreive the kernel arguments and entry point. */
	movl	4(%esp), %edi
	movl	8(%esp), %esi
	movl	16(%esp), %eax

	/* Set the stack pointer to the top of the stack. */
	addl	$PAGE_SIZE, %esp
	andl	$0xFFFFF000, %esp

	/* Clear the stack frame. */
	xorl	%ebp, %ebp

	/* Call the kernel. */
	push	%esi
	push	%edi
	call	*%eax
.size arch_enter_kernel32, .-arch_enter_kernel32
