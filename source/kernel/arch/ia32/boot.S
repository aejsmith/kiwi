/* Kiwi IA32 boot code.
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
 * @brief		IA32 boot code.
 */

#include <arch/descriptor.h>
#include <arch/memmap.h>
#include <arch/page.h>
#include <arch/stack.h>
#include <arch/x86/features.h>
#include <arch/x86/sysreg.h>

#include <platform/multiboot.h>

/** Macro to define a variable. */
.macro .defvar name, len, global, align
.if \align
	.align \align
.endif
.if \global
	.global \name
.endif
.type \name, @object
\name:
	.fill	\len
.size \name, .-\name
.endm

.section .multiboot, "ax", @progbits

/** Multiboot header structure. */
multiboot_header:
	.long MB_HEADER_MAGIC
	.long (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO)
	.long -(MB_HEADER_MAGIC + (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO))
.size multiboot_header, .-multiboot_header

.section .init.text, "ax", @progbits

/** Kernel entry point (BSP).
 *
 * Main entry point of the kernel. When this function is called, the CPU is in
 * 32-bit protected mode at the kernel's physical location. This function sets
 * up paging structures using PAE if available, and then calls the proper
 * kernel main function.
 *
 * @param eax		Multiboot magic number.
 * @param ebx		Multiboot information structure pointer.
 */
.global __kernel_entry
.type __kernel_entry, @function
__kernel_entry:
	/* Set the stack pointer to the physical location of our stack. */
	movl	$KA2PA(__boot_stack) + KSTACK_SIZE, %esp

	/* Clear EFLAGS. */
	push	$0
	popf

	/* Check the Multiboot magic number. */
	cmpl	$MB_LOADER_MAGIC, %eax
	jne	.Lmagic_err

	/* Save the Multiboot information pointer. */
	movl	%ebx, KA2PA(__mb_info)

	/* Set up the temporary GDT and jump to the kernel code segment. */
	lgdt	KA2PA(__boot_gdtp)
	mov	$SEG_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	ljmp	$SEG_K_CS, $KA2PA(1f)
1:
	/* Check for CPUID - if we can change EFLAGS.ID, it is supported. */
	pushf
	pop	%eax
	mov	%eax, %ebx
	xor	$SYSREG_FLAGS_ID, %eax
	push	%eax
	popf
	pushf
	pop	%eax
	cmp	%eax, %ebx
	jz	.Lcpuid_err

	/* Check for PAE/PGE support. */
	movl	$CPUID_FEATURE_INFO, %eax
	cpuid
	bt	$6, %edx
	jnc	.Lpae_err
	bt	$13, %edx
	jnc	.Lpge_err
	bt	$24, %edx
	jnc	.Lfxsr_err

	/* Generate the temporary identity mapping page directory. */
	movl    $(PG_PRESENT | PG_WRITE | PG_LARGE), %eax
	movl    $512, %ecx
	movl    $0, %edx
2:	movl    %eax, KA2PA(__ident_pdir)(,%edx,8)
	movl    $0, KA2PA(__ident_pdir) + 4(,%edx,8)
	addl    $0x200000, %eax
	incl    %edx
	loop    2b

	/* Enable PAE/PGE/OSFXSR. Also enable PSE because although its not
	 * necessary when PAE is being used, VirtualBox shits itself without
	 * it, even though every other x86 box I have doesn't. */
	movl	%cr4, %eax
	orl	$(SYSREG_CR4_PAE | SYSREG_CR4_PGE | SYSREG_CR4_PSE | SYSREG_CR4_OSFXSR), %eax
	movl	%eax, %cr4

	/* Initialise the FPU. */
	fninit

	/* Point CR3 to the boot PDP. */
	movl	$KA2PA(__boot_pdp), %eax
	movl	%eax, %cr3

	/* Set PG/WP/NE/MP in CR0 (Paging Enable, Write Protect, Numeric Error,
	 * Monitor Coprocessor). */
	movl	%cr0, %ecx
	orl	$(SYSREG_CR0_PG | SYSREG_CR0_WP | SYSREG_CR0_NE | SYSREG_CR0_MP), %ecx
	movl	%ecx, %cr0

	/* Jump to the kernel virtual address. */
	leal	2f, %eax
	jmp	*%eax
2:
	/* Now update the stack pointer. */
	movl	$__boot_stack + KSTACK_SIZE, %esp

	/* Clear the stack frame. */
	xorl	%ebp, %ebp

	/* Get the Multiboot info structure pointer. */
	movl	(__mb_info), %edi

	/* Call C main function. */
	push	%edi
	call	kmain_bsp

	/* If we got here something has gone wrong. */
3:	ud2a
	jmp	3b
.Lmagic_err:
	movl	$KA2PA(magic_err_str), %esi
	jmp	.Learly_die
.Lcpuid_err:
	movl	$KA2PA(cpuid_err_str), %esi
	jmp	.Learly_die
.Lpae_err:
	movl	$KA2PA(pae_err_str), %esi
	jmp	.Learly_die
.Lpge_err:
	movl	$KA2PA(pge_err_str), %esi
	jmp	.Learly_die
.Lfxsr_err:
	movl	$KA2PA(fxsr_err_str), %esi
	jmp	.Learly_die
.Learly_die:
	/* Print an error message. String address should be in %esi. */
	movl	$0xB8000, %edi
4:	xorl	%eax, %eax
	lodsb
	test	%al, %al
	jz	5f
	or	$0xD00, %ax
	stosw
	jmp	4b
5:	jmp	5b
.size __kernel_entry, .-__kernel_entry

/** Kernel entry point (AP).
 *
 * Main entry point of the kernel for an AP. When this function is called, the
 * CPU is in 32-bit protected mode at the kernel's physical location. This
 * function loads the page directory or PDP and calls the real main function.
 *
 * @todo		Check for required CPU features.
 */
.global __kernel_ap_entry
.type __kernel_ap_entry, @function
__kernel_ap_entry:
	/* Set proper segments. */
	lgdt	KA2PA(__boot_gdtp)
	mov	$SEG_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	ljmp	$SEG_K_CS, $KA2PA(1f)
1:
	/* Enable PAE/PGE/PSE/OSFXSR. */
	movl	%cr4, %eax
	orl	$(SYSREG_CR4_PAE | SYSREG_CR4_PGE | SYSREG_CR4_PSE | SYSREG_CR4_OSFXSR), %eax
	movl	%eax, %cr4

	/* Initialise the FPU. */
	fninit

	/* Create temporary identity mapping. */
	movl	$KA2PA(__ident_pdir) + PG_PRESENT, KA2PA(__boot_pdp)

	/* Point CR3 to the boot PDP. */
	movl	$KA2PA(__boot_pdp), %eax
	movl	%eax, %cr3
#if CONFIG_X86_NX
	/* Check for NX support and enable it if necessary. */
	movl	$CPUID_EXT_FEATURE, %eax
	cpuid
	bt	$20, %edx
	jnc	2f
	movl	$SYSREG_MSR_EFER, %ecx
	rdmsr
	orl	$SYSREG_EFER_NXE, %eax
	wrmsr
2:
#endif
	/* Set PG/WP/NE/MP in CR0 (Paging Enable, Write Protect, Numeric Error,
	 * Monitor Coprocessor). */
	movl	%cr0, %ecx
	orl	$(SYSREG_CR0_PG | SYSREG_CR0_WP | SYSREG_CR0_NE | SYSREG_CR0_MP), %ecx
	movl	%ecx, %cr0

	/* Jump to the kernel virtual address. */
	leal	3f, %eax
	jmp	*%eax
3:
	/* Set the stack pointer. */
	movl	(ap_stack_ptr), %esp

	/* Unmap identity map page table and flush TLB. */
	movl	$0, KA2PA(__boot_pdp)
	movl	$KA2PA(__boot_pdp), %eax
	movl	%eax, %cr3

	/* Clear EFLAGS. */
	push	$0
	popf

	/* Clear the stack frame. */
	xorl	%ebp, %ebp

	/* Call C main function. */
	call	kmain_ap

	/* If we got here something has gone wrong. */
4:	ud2a
	jmp	4b
.size __kernel_ap_entry, .-__kernel_ap_entry

.section .init.data, "aw", @progbits

/** Identity map page directory. */
.defvar __ident_pdir, PAGE_SIZE, 0, PAGE_SIZE

.section .data, "aw", @progbits

/** Kernel page directory. Last entry maps onto itself to provide access to
 *  kernel page tables. */
.align PAGE_SIZE
.global __kernel_pdir
.type __kernel_pdir, @object
__kernel_pdir:
	.fill	510, 8, 0
	.long	KERNEL_PHYS_BASE + (PG_PRESENT | PG_WRITE | PG_LARGE | PG_GLOBAL)
	.long	0
	.long	KA2PA(__kernel_pdir) + (PG_PRESENT | PG_WRITE | PG_GLOBAL)
	.long	0
.size __kernel_pdir, .-__kernel_pdir

/** Initial PDP. Can't use .quad in here because AS chokes on it. */
.align PAGE_SIZE
.global __boot_pdp
.type __boot_pdp, @object
__boot_pdp:
	.long	KA2PA(__ident_pdir) + PG_PRESENT
	.long	0
	.fill	2, 8, 0
	.long	KA2PA(__kernel_pdir) + PG_PRESENT
	.long	0
.size __boot_pdp, .-__boot_pdp

.section .rodata, "a", @progbits

magic_err_str:		.asciz "Multiboot magic number is invalid."
cpuid_err_str:		.asciz "CPU does not support CPUID."
pae_err_str:		.asciz "CPU does not support PAE."
pge_err_str:		.asciz "CPU does not support PGE."
fxsr_err_str:		.asciz "CPU does not support FXSR."

.section .bss, "aw", @nobits

/** Multiboot information pointer. */
.defvar __mb_info, 4, 0, 0

/** Initial stack for the kernel. */
.defvar __boot_stack, KSTACK_SIZE, 1, PAGE_SIZE
