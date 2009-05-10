/* Kiwi AMD64 startup code.
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
 * @brief		AMD64 startup code.
 */

#include <arch/defs.h>
#include <arch/mem.h>
#include <arch/multiboot.h>
#include <arch/page.h>
#include <arch/segment.h>

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
	.long (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO|MB_HFLAG_KLUDGE)
	.long -(MB_HEADER_MAGIC + (MB_HFLAG_MODALIGN|MB_HFLAG_MEMINFO|MB_HFLAG_KLUDGE))
	.long KA2PA(multiboot_header)
	.long KA2PA(__text_start)
	.long KA2PA(__data_end)
	.long KA2PA(__end)
	.long KA2PA(__kernel_entry)
.size multiboot_header, .-multiboot_header

.section .init.text, "ax", @progbits

/** Kernel entry point (BSP).
 *
 * Main entry point of the kernel. When this function is called, the CPU is in
 * 32-bit protected mode at the kernel's physical location. This function sets
 * up paging structures, enters long mode, and then calls the proper kernel
 * main function.
 *
 * @param eax		Multiboot magic number.
 * @param ebx		Multiboot information structure pointer.
 */
.global __kernel_entry
.type __kernel_entry, @function
__kernel_entry:
.code32
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

	/* Set up the temporary GDT and jump to the 32-bit code segment. */
	lgdt	KA2PA(__boot_gdtp)
	mov	$SEG_K_DS32, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	ljmp	$SEG_K_CS32, $KA2PA(1f)
1:
	/* Check for CPUID - if we can change EFLAGS.ID, it is supported. */
	pushf
	pop	%eax
	mov	%eax, %ebx
	xor	$X86_FLAGS_ID, %eax
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

	/* Check for long mode support. */
	movl	$CPUID_EXT_FEATURE, %eax
	cpuid
	bt	$29, %edx
	jnc	.Llmode_err

	/* Generate the physical mapping page directories. */
	movl    $(PG_PRESENT | PG_WRITE | PG_LARGE | PG_GLOBAL), %eax
	movl    $2048, %ecx
	movl    $0, %edx
2:	movl    %eax, KA2PA(__pmap_pdirs)(,%edx,8)
	movl    $0, KA2PA(__pmap_pdirs) + 4(,%edx,8)
	addl    $0x200000, %eax
	incl    %edx
	loop    2b

	/* Enable PAE/PGE (CR4.PAE, bit 5/CR4.PGE, bit 7). */
	movl	%cr4, %eax
	orl	$(X86_CR4_PAE | X86_CR4_PGE), %eax
	movl	%eax, %cr4

	/* Point CR3 to the boot PML4. */
	movl	$KA2PA(__boot_pml4), %eax
	movl	%eax, %cr3

	/* Enable long mode by setting EFER.LME. */
	movl	$X86_MSR_IA32_EFER, %ecx
	rdmsr
	orl	$X86_EFER_LME, %eax
	wrmsr

	/* Set PG/WP/NE/MP in CR0 (Paging Enable, Write Protect, Numeric Error,
	 * Monitor Coprocessor). Paging will put us in compatibility mode. */
	movl	%cr0, %ecx
	orl	$(X86_CR0_PG | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP), %ecx
	movl	%ecx, %cr0

	/* Jump into the 64-bit code segment. */
	ljmp	$SEG_K_CS, $KA2PA(.Llmode)
.Lmagic_err:
	movl	$0xB8000, %edi
	movl	$KA2PA(magic_err_str), %esi
	jmp	.Learly_die
.Lcpuid_err:
	movl	$0xB8000, %edi
	movl	$KA2PA(cpuid_err_str), %esi
	jmp	.Learly_die
.Lpae_err:
	movl	$0xB8000, %edi
	movl	$KA2PA(pae_err_str), %esi
	jmp	.Learly_die
.Lpge_err:
	movl	$0xB8000, %edi
	movl	$KA2PA(pge_err_str), %esi
	jmp	.Learly_die
.Llmode_err:
	movl	$0xB8000, %edi
	movl	$KA2PA(lmode_err_str), %esi
	jmp	.Learly_die
.Learly_die:
	/* Print an error message. String address should be in %esi. */
	xorl	%eax, %eax
	lodsb
	test	%al, %al
	jz	1f
	or	$0xD00, %ax
	stosw
	jmp	.Learly_die
1:	jmp	1b
.align 8
.code64
.Llmode:
	/* Jump to the virtual location of the kernel. */
	leaq	2f, %rax
	jmp	*%rax
2:
	/* Set data segments. */
	mov	$SEG_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Now update the stack pointer. */
	movq	$__boot_stack + KSTACK_SIZE, %rsp

	/* Clear the stack frame. */
	xorq	%rbp, %rbp

	/* Get the Multiboot info structure pointer. */
	xorq	%rdi, %rdi
	movl	(__mb_info), %edi

	/* Call C main function. */
	//call	kmain_bsp

	/* If we got here something has gone wrong. */
1:	//ud2a
	jmp	1b
.size __kernel_entry, .-__kernel_entry

#if CONFIG_SMP
/** Kernel entry point (AP).
 *
 * Main entry point of the kernel for an AP. When this function is called, the
 * CPU is in 32-bit protected mode at the kernel's physical location. This
 * function loads the PML4, enters long mode, and then calls the proper kernel
 * main function.
 *
 * @todo		Check for required CPU features.
 */
.global __kernel_ap_entry
.type __kernel_ap_entry, @function
__kernel_ap_entry:
.code32
	/* Set proper segments. */
	lgdt	KA2PA(__boot_gdtp)
	mov	$SEG_K_DS32, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss
	ljmp	$SEG_K_CS32, $KA2PA(1f)
1:
	/* Enable PAE/PGE (CR4.PAE, bit 5/CR4.PGE, bit 7). */
	movl	%cr4, %eax
	orl	$(X86_CR4_PAE | X86_CR4_PGE), %eax
	movl	%eax, %cr4

	/* Create temporary identity mapping. */
	movl	$KA2PA(__kernel_pdp) + (PG_PRESENT | PG_WRITE), KA2PA(__boot_pml4)

	/* Point CR3 to the boot PML4. */
	movl	$KA2PA(__boot_pml4), %eax
	movl	%eax, %cr3

	/* Enable long mode by setting EFER.LME. */
	movl	$X86_MSR_IA32_EFER, %ecx
	rdmsr
	orl	$X86_EFER_LME, %eax
	wrmsr
#if CONFIG_X86_NX
	/* Check for NX support and enable it if necessary. */
	movl	$CPUID_EXT_FEATURE, %eax
	cpuid
	bt	$20, %edx
	jnc	2f
	movl	$X86_MSR_IA32_EFER, %ecx
	rdmsr
	orl	$X86_EFER_NXE, %eax
	wrmsr
2:
#endif
	/* Set PG/WP/NE/MP in CR0 (Paging Enable, Write Protect, Numeric Error,
	 * Monitor Coprocessor). Paging will put us in compatibility mode. */
	movl	%cr0, %ecx
	orl	$(X86_CR0_PG | X86_CR0_WP | X86_CR0_NE | X86_CR0_MP), %ecx
	movl	%ecx, %cr0

	/* Jump into the 64-bit code segment. */
	ljmp	$SEG_K_CS, $KA2PA(2f)
.align 8
.code64
2:
	/* Jump to the virtual location of the kernel. */
	leaq	3f, %rax
	jmp	*%rax
3:
	/* Set data segment. */
	mov	$SEG_K_DS, %ax
	mov	%ax, %ds
	mov	%ax, %es
	mov	%ax, %fs
	mov	%ax, %gs
	mov	%ax, %ss

	/* Set the stack pointer. */
	movq	(ap_stack_ptr), %rsp

	/* Unmap identity map PML4 entry and flush TLB. */
	movq	$0, (__boot_pml4)
	movq	$KA2PA(__boot_pml4), %rax
	movq	%rax, %cr3

	/* Clear RFLAGS. */
	push	$0
	popf

	/* Clear the stack frame. */
	xorq	%rbp, %rbp

	/* Call C main function. */
	call	kmain_ap

	/* If we got here something has gone wrong. */
1:	ud2a
	jmp	1b
.size __kernel_ap_entry, .-__kernel_ap_entry
#endif

.section .data, "aw", @progbits

/** Physical map page directories. Map 4GB to begin with, anything higher is
 *  mapped in as required by the pagefault handler. */
.align PAGE_SIZE
.global __pmap_pdirs
.type __pmap_pdirs, @object
__pmap_pdirs:
	.fill	2048, 8, 0
.size __pmap_pdirs, .-__pmap_pdirs

/** Kernel page directory. Assume kernel is under 2MB for now. */
.align PAGE_SIZE
.global __kernel_pdir
.type __kernel_pdir, @object
__kernel_pdir:
	.quad	KERNEL_PHYS_BASE + (PG_PRESENT | PG_WRITE | PG_LARGE | PG_GLOBAL)
	.fill	511, 8, 0
.size __kernel_pdir, .-__kernel_pdir

/** Kernel PDP. */
.align PAGE_SIZE
.global __kernel_pdp
.type __kernel_pdp, @object
__kernel_pdp:
	.quad	KA2PA(__pmap_pdirs) + 0x0000 + (PG_PRESENT | PG_WRITE)
	.quad	KA2PA(__pmap_pdirs) + 0x1000 + (PG_PRESENT | PG_WRITE)
	.quad	KA2PA(__pmap_pdirs) + 0x2000 + (PG_PRESENT | PG_WRITE)
	.quad	KA2PA(__pmap_pdirs) + 0x3000 + (PG_PRESENT | PG_WRITE)
	.fill	506, 8, 0
	.quad	KA2PA(__kernel_pdir) + (PG_PRESENT | PG_WRITE)
	.fill	1, 8, 0
.size __kernel_pdp, .-__kernel_pdp

/** Initial PML4. */
.align PAGE_SIZE
.global __boot_pml4
.type __boot_pml4, @object
__boot_pml4:
	.quad	KA2PA(__kernel_pdp) + (PG_PRESENT | PG_WRITE)
	.fill	510, 8, 0
	.quad	KA2PA(__kernel_pdp) + (PG_PRESENT | PG_WRITE)
.size __boot_pml4, .-__boot_pml4

.section .rodata, "a", @progbits

magic_err_str:		.asciz "Multiboot magic number is invalid."
cpuid_err_str:		.asciz "CPU does not support CPUID."
pae_err_str:		.asciz "CPU does not support PAE."
pge_err_str:		.asciz "CPU does not support PGE."
lmode_err_str:		.asciz "CPU does not support long mode. Use a 32-bit version of Exclaim."

.section .bss, "aw", @nobits

/** Multiboot information pointer. */
.defvar __mb_info, 4, 0, 0

/** Initial stack for the kernel. */
.defvar __boot_stack, KSTACK_SIZE, 1, PAGE_SIZE
