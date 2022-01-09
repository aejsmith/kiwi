/*
 * Copyright (C) 2009-2022 Alex Smith
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
 * @brief               AMD64 secondary CPU boot code.
 *
 * The layout of this code has several special locations that the kernel stores
 * various bits of information for us at:
 *  16 - entry_addr - 8 bytes - Kernel entry point address.
 *  24 - entry_arg  - 8 bytes - Argument to entry function.
 *  32 - kernel_sp  - 8 bytes - Kernel stack pointer.
 *  40 - kernel_cr3 - 4 bytes - Address to load into CR3.
 */

#include <x86/asm.h>
#include <x86/cpu.h>

.code16

FUNCTION_START(_start)
    jmp     ap_boot
FUNCTION_END(_start)

.align 16

entry_addr: .quad 0
entry_arg:  .quad 0
kernel_sp:  .quad 0
kernel_cr3: .long 0

.align 8

PRIVATE_FUNCTION_START(ap_boot)
    xorl    %esi, %esi

    /* Set the data segment and save the load location. */
    mov     %cs, %si
    mov     %si, %ds
    shl     $4, %esi

    /* Set the correct base address of the GDT and load it. */
    leal    __gdt(%esi), %eax
    movl    %eax, (__gdtp + 2)
    lgdtl   (__gdtp)

    /* Enable protected mode. */
    movl    %cr0, %eax
    orl     $X86_CR0_PE, %eax
    movl    %eax, %cr0

    /* Jump into the 32-bit code segment. */
    leal    .Lpmode(%esi), %eax
    movl    %eax, (1f + 2)
1:  ljmpl   $0x08, $0
.align 8
.Lpmode:
.code32
    /* Load data segment. */
    movl    $0x10, %eax
    mov     %ax, %ds

    /* Enable PAE/PGE. */
    movl    %cr4, %eax
    orl     $(X86_CR4_PAE | X86_CR4_PGE), %eax
    movl    %eax, %cr4

    /* Load the correct value for CR3. */
    movl    kernel_cr3(%esi), %eax
    movl    %eax, %cr3

    /* Enable long mode by setting EFER.LME. */
    movl    $X86_MSR_EFER, %ecx
    rdmsr
    orl     $X86_EFER_LME, %eax
    wrmsr

    /* Check for NX support and enable it if necessary. We must do this
     * here as if it is supported the page tables will have the NX bit set
     * on certain parts of the kernel. */
    movl    $X86_CPUID_EXT_FEATURE, %eax
    cpuid
    bt      $20, %edx
    jnc     2f
    movl    $X86_MSR_EFER, %ecx
    rdmsr
    orl     $X86_EFER_NXE, %eax
    wrmsr
2:
    /* Set PG (Paging Enable) to put us in compatibility mode. */
    movl    %cr0, %eax
    orl     $X86_CR0_PG, %eax
    movl    %eax, %cr0

    /* Jump into the 64-bit code segment. */
    leal    .Llmode(%esi), %eax
    movl    %eax, (1f + 1)(%esi)
1:  ljmpl   $0x18, $0
.align 8
.code64
.Llmode:
    /* Set data segments. */
    mov     $0x20, %ax
    mov     %ax, %ss
    xorl    %eax, %eax
    mov     %ax, %ds
    mov     %ax, %es
    mov     %ax, %fs
    mov     %ax, %gs

    /* Clear top 32 bits of our load address. */
    movq    %rsi, %rsi

    /* Load the stack address. */
    movq    kernel_sp(%rsi), %rsp

    /* Clear the stack frame/RFLAGS. */
    xorq    %rbp, %rbp
    push    $0
    popf

    /* Call the kernel. */
    movq    entry_arg(%rsi), %rdi
    call    *entry_addr(%rsi)
FUNCTION_END(ap_boot)

__gdt:
    .quad 0x0000000000000000        /**< NULL descriptor (0x00). */
    .quad 0x00cf9a000000ffff        /**< 32-bit code     (0x08). */
    .quad 0x00cf92000000ffff        /**< 32-bit data     (0x10). */
    .quad 0x00af9a000000ffff        /**< 64-bit code     (0x18). */
    .quad 0x008f92000000ffff        /**< 64-bit data     (0x20). */
.L__gdt_end:

__gdtp:
    .word .L__gdt_end-__gdt-1
    .long 0
