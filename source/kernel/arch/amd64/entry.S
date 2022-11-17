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
 * @brief               AMD64 kernel entry points.
 */

#include <arch/frame.h>
#include <arch/thread.h>

#include <x86/asm.h>
#include <x86/cpu.h>
#include <x86/descriptor.h>

#include <status.h>

/** Push registers onto the stack. */
.macro PUSH_REGS
    push    %rax
    push    %rbx
    push    %rcx
    push    %rdx
    push    %rdi
    push    %rsi
    push    %rbp
    push    %r8
    push    %r9
    push    %r10
    push    %r11
    push    %r12
    push    %r13
    push    %r14
    push    %r15
.endm

.macro PUSH_REGS_CFI
    .cfi_remember_state
    .cfi_def_cfa    rsp, FRAME_SIZE
    .cfi_rel_offset rsp, FRAME_OFF_SP
    .cfi_rel_offset rip, FRAME_OFF_IP
    .cfi_rel_offset rax, FRAME_OFF_AX
    .cfi_rel_offset rbx, FRAME_OFF_BX
    .cfi_rel_offset rcx, FRAME_OFF_CX
    .cfi_rel_offset rdx, FRAME_OFF_DX
    .cfi_rel_offset rdi, FRAME_OFF_DI
    .cfi_rel_offset rsi, FRAME_OFF_SI
    .cfi_rel_offset rbp, FRAME_OFF_BP
    .cfi_rel_offset r8,  FRAME_OFF_R8
    .cfi_rel_offset r9,  FRAME_OFF_R9
    .cfi_rel_offset r10, FRAME_OFF_R10
    .cfi_rel_offset r11, FRAME_OFF_R11
    .cfi_rel_offset r12, FRAME_OFF_R12
    .cfi_rel_offset r13, FRAME_OFF_R13
    .cfi_rel_offset r14, FRAME_OFF_R14
    .cfi_rel_offset r15, FRAME_OFF_R15
.endm

/** Pop registers from the stack. */
.macro POP_REGS
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %r11
    pop     %r10
    pop     %r9
    pop     %r8
    pop     %rbp
    pop     %rsi
    pop     %rdi
    pop     %rdx
    pop     %rcx
    pop     %rbx
    pop     %rax
.endm

.macro POP_REGS_CFI
    .cfi_restore_state
.endm

/**
 * Macro to define an ISR.
 *
 * Aligned to 16 bytes because the IDT initialization code requires each handler
 * to be 16 bytes.
 *
 * @param nr            Interrupt vector number.
 */
.macro ISR nr
.align 16
    push    $0
    push    $\nr
    jmp     isr_common
.endm

/**
 * Macro to define an ISR with an error code.
 *
 * Aligned to 16 bytes because the IDT initialization code requires each handler
 * to be 16 bytes.
 *
 * @param nr            Interrupt vector number.
 */
.macro ISR_E nr
.align 16
    push    $\nr
    jmp     isr_common
.endm

/** Array of ISR handlers, each 16 bytes long. */
.align 16
FUNCTION_START(isr_array)
    /* Define the exceptions (0-19) and the reserved interrupts (20-31). */
    ISR     0
    ISR     1
    ISR     2
    ISR     3
    ISR     4
    ISR     5
    ISR     6
    ISR     7
    ISR_E   8
    ISR     9
    ISR_E   10
    ISR_E   11
    ISR_E   12
    ISR_E   13
    ISR_E   14
    ISR     15
    ISR     16
    ISR_E   17
    ISR     18
    ISR     19
    ISR     20
    ISR     21
    ISR     22
    ISR     23
    ISR     24
    ISR     25
    ISR     26
    ISR     27
    ISR     28
    ISR     29
    ISR     30
    ISR     31

    /* Define the user-defined ISRs (32-255) - none take an error code. */
    .Lintr = 32
    .rept 224
        ISR .Lintr
        .Lintr = .Lintr+1
    .endr
FUNCTION_END(isr_array)

/** Common ISR handling code. */
PRIVATE_FUNCTION_START(isr_common)
    .cfi_startproc simple
    .cfi_def_cfa rsp, 0
    /* This is necessary when tracing into user code to stop GDB from bailing
     * out because the new stack pointer is lower than the previous one. */
    .cfi_signal_frame

    /* If coming from user-mode, need to load the kernel GS segment base. */
    testl   $3, 24(%rsp)
    jz      1f
    swapgs
1:
    /* Create the interrupt frame structure on the stack. */
    PUSH_REGS
    PUSH_REGS_CFI

    /* Clear direction flag. */
    cld

    /* Set up a standard stack frame so we can backtrace over this. */
    movq    %rsp, %rdi
    movq    FRAME_OFF_IP(%rsp), %rax
    pushq   %rax
    pushq   %rbp
    .cfi_adjust_cfa_offset 16
    movq    %rsp, %rbp

    /* Call the interrupt handler. */
    call    interrupt_handler

    /* Restore the saved registers. */
    addq    $16, %rsp
    .cfi_adjust_cfa_offset -16
    POP_REGS
    POP_REGS_CFI

    /* Get rid of the error code and interrupt number, restore the previous GS
     * base if returning to user-mode, and return. */
    addq    $16, %rsp
    testl   $3, 8(%rsp)
    jz      2f
    swapgs
2:  iretq
    .cfi_endproc
FUNCTION_END(isr_common)

/** SYSCALL entry point. */
FUNCTION_START(syscall_entry)
    .cfi_startproc simple
    .cfi_def_cfa rsp, 0
    /* See isr_common. */
    .cfi_signal_frame

    /* Swap to the kernel GS base address, which points at the current CPU's
     * architecture data. The second pointer in this is our kernel stack
     * pointer. The third pointer is a temporary scratch space for us to save
     * the userspace stack pointer in before we push it to the stack. */
    swapgs
    movq    %rsp, %gs:ARCH_THREAD_OFF_USER_RSP
    movq    %gs:ARCH_THREAD_OFF_KERNEL_RSP, %rsp

    /* Get back the userspace stack pointer from the temporary scratch space
     * and put it onto the kernel stack. */
    push    $(USER_DS | 3)
    push    %gs:ARCH_THREAD_OFF_USER_RSP

    /* We now have a stack pointer set up, push information SYSCALL saved for us
     * (R11 = original RFLAGS, RCX = return IP) and the rest of the interrupt
     * frame. */
    push    %r11
    push    $(USER_CS | 3)
    push    %rcx
    push    $0
    push    $0
    PUSH_REGS
    PUSH_REGS_CFI

    /* Save the user-mode interrupt frame pointer. */
    movq    %rsp, %gs:ARCH_THREAD_OFF_USER_FRAME

    /* Get system call table entry. */
    xorq    %rbx, %rbx
    movq    FRAME_OFF_AX(%rsp), %rax
    cmpq    syscall_table_size, %rax
    jae     1f
    shlq    $4, %rax
    leaq    syscall_table(%rax), %rbx
1:
    /* Perform kernel entry work and enable interrupts. */
    movq    %rbx, %rdi
    call    thread_at_kernel_entry
    sti

    /* If it was invalid, return that. */
    testq   %rbx, %rbx
    jz      .Linvalid_syscall

    /* Check the argument count. If there are more than 6 arguments, we must
     * copy from the stack. */
    movq    8(%rbx), %rdx
    cmpq    $6, %rdx
    ja      .Lstack_args

    /* Restore arguments and perform the call. The argument that should be in
     * RCX is passed in R10, as RCX is used by SYSCALL/SYSRET as the return IP. */
    movq    FRAME_OFF_DI(%rsp), %rdi
    movq    FRAME_OFF_SI(%rsp), %rsi
    movq    FRAME_OFF_DX(%rsp), %rdx
    movq    FRAME_OFF_R10(%rsp), %rcx
    movq    FRAME_OFF_R8(%rsp), %r8
    movq    FRAME_OFF_R9(%rsp), %r9
    call    *(%rbx)
.Lreturn:
    /* Save the return value of the system call, unless we're returning out of
     * kern_thread_restore(), in which case we shouldn't clobber the restored
     * RAX value. */
    testq   $ARCH_THREAD_FRAME_RESTORED, %gs:ARCH_THREAD_OFF_FLAGS
    jnz     1f
    movq    %rax, FRAME_OFF_AX(%rsp)
1:
    /* Disable interrupts and perform kernel exit work. */
    cli
    movq    %rbx, %rdi
    movq    %rax, %rsi
    call    thread_at_kernel_exit

    /* If we're going to execute a user-mode interrupt handler, or we have
     * returned from one, the interrupt frame setup code will have set the
     * ARCH_THREAD_FRAME_MODIFIED flag. When this flag is set, we return via
     * IRET as it doesn't clobber certain registers. */
    testq   $ARCH_THREAD_FRAME_MODIFIED, %gs:ARCH_THREAD_OFF_FLAGS
    jnz     .Liret

    /* Restore saved registers, RFLAGS/RIP for SYSRET, and the userspace stack
     * pointer. */
    POP_REGS
    POP_REGS_CFI
    add     $16, %rsp
    pop     %rcx
    add     $8, %rsp
    pop     %r11
    pop     %rsp

    /* Restore previous GS base and return to user mode. */
    swapgs
    sysretq
.Lstack_args:
    /* Work out how many bytes to copy and reserve space on the stack.
     *  RDX = argument count, RBX = syscall_t pointer, R12 = saved SP.
     * RDX is in the correct location to pass to memcpy_from_user() after
     * modifying, RBX and R12 are callee-save so they won't be clobbered by any
     * called functions. */
    sub     $6, %rdx
    shl     $3, %rdx
    movq    %rsp, %r12
    sub     %rdx, %rsp

    /* Copy the arguments. The source is the userspace stack pointer + 8. */
    movq    %rsp, %rdi
    movq    FRAME_OFF_SP(%r12), %rsi
    add     $8, %rsi
    call    memcpy_from_user
    cmp     $STATUS_SUCCESS, %eax
    jne     .Lcopy_failed

    /* Restore the arguments passed in registers and perform the call. */
    movq    FRAME_OFF_DI(%r12), %rdi
    movq    FRAME_OFF_SI(%r12), %rsi
    movq    FRAME_OFF_DX(%r12), %rdx
    movq    FRAME_OFF_R10(%r12), %rcx
    movq    FRAME_OFF_R8(%r12), %r8
    movq    FRAME_OFF_R9(%r12), %r9
    call    *(%rbx)

    /* Restore stack pointer then return. */
    movq    %r12, %rsp
    jmp     .Lreturn
.Liret:
    /* Clear the modified/restored flags. */
    andq    $~(ARCH_THREAD_FRAME_MODIFIED | ARCH_THREAD_FRAME_RESTORED), %gs:ARCH_THREAD_OFF_FLAGS

    /* Restore the registers, get rid of interrupt/error code. */
    POP_REGS
    addq    $16, %rsp

    /* Return to user mode. */
    swapgs
    iretq
.Lcopy_failed:
    movq    $STATUS_INVALID_ADDR, %rax
    movq    %r12, %rsp
    jmp     .Lreturn
.Linvalid_syscall:
    movq    $STATUS_INVALID_SYSCALL, %rax
    jmp     .Lreturn

    .cfi_endproc
FUNCTION_END(syscall_entry)

/** Enter user mode in the current thread.
 * @param frame         Previously prepared frame. */
FUNCTION_START(arch_thread_user_enter)
    /* Point SP at the frame to restore. */
    movq    %rdi, %rsp

    /* Restore the registers, get rid of interrupt/error code. */
    POP_REGS
    addq    $16, %rsp

    swapgs
    iretq
FUNCTION_END(arch_thread_user_enter)
