/*
 * Copyright (C) 2009-2023 Alex Smith
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
 * @brief               ARM64 kernel entry points.
 */

#include <arch/frame.h>
#include <arch/thread.h>

#include <arm64/asm.h>
#include <arm64/cpu.h>

#include <status.h>

.section .text, "ax", @progbits

/** Call an interrupt handler function. */
.macro CALL_HANDLER, name
    /* Pass frame to handler. */
    mov     x0, sp

    /* Set up frame pointer (X29) with ELR as return address. */
    stp     x29, x1, [sp, #-16]!
    mov     x29, sp

    bl      \name

    add     sp, sp, #16
.endm

/** Push a frame_t on the stack, and leave a pointer to it in x19. */
.macro PUSH_REGS, user
    /* X0-29. */
    stp     x0, x1, [sp,#-16]!
    stp     x2, x3, [sp,#-16]!
    stp     x4, x5, [sp,#-16]!
    stp     x6, x7, [sp,#-16]!
    stp     x8, x9, [sp,#-16]!
    stp     x10, x11, [sp,#-16]!
    stp     x12, x13, [sp,#-16]!
    stp     x14, x15, [sp,#-16]!
    stp     x16, x17, [sp,#-16]!
    stp     x18, x19, [sp,#-16]!
    stp     x20, x21, [sp,#-16]!
    stp     x22, x23, [sp,#-16]!
    stp     x24, x25, [sp,#-16]!
    stp     x26, x27, [sp,#-16]!
    stp     x28, x29, [sp,#-16]!

    /* X30/SP. */
.if \user
    /* Save the EL0 SP. */
    mrs     x9, sp_el0
.else
    /* Restore original SP value to save. */
    add     x9, sp, #(30 * 8)
.endif
    stp     x30, x9, [sp,#-16]!

    /* SPSR/ELR. */
    mrs     x9, spsr_el1
    mrs     x10, elr_el1
    stp     x9, x10, [sp,#-16]!

    mov     x19, sp

.if \user
    /* Save the frame pointer in curr_thread. */
    mrs     x9, tpidr_el1
    str     x19, [x9,#ARCH_THREAD_OFF_user_frame]
.endif
.endm

/** Restore frame at current stack pointer. */
.macro POP_REGS, user
    /* SPSR/ELR. */
    ldp     x9, x10, [sp], #16
    msr     spsr_el1, x9
    msr     elr_el1, x10

    /* X30/SP. */
    ldp     x30, x9, [sp], #16
.if \user
    /* Restore EL0 SP for user. */
    msr     sp_el0, x9
.else
    /* For the kernel, SP doesn't need to be restored here, we'll get it back
     * by the end. */
.endif

    /* X0-29. */
    ldp     x28, x29, [sp], #16
    ldp     x26, x27, [sp], #16
    ldp     x24, x25, [sp], #16
    ldp     x22, x23, [sp], #16
    ldp     x20, x21, [sp], #16
    ldp     x18, x19, [sp], #16
    ldp     x16, x17, [sp], #16
    ldp     x14, x15, [sp], #16
    ldp     x12, x13, [sp], #16
    ldp     x10, x11, [sp], #16
    ldp     x8, x9, [sp], #16
    ldp     x6, x7, [sp], #16
    ldp     x4, x5, [sp], #16
    ldp     x2, x3, [sp], #16
    ldp     x0, x1, [sp], #16
.endm

PRIVATE_FUNCTION_START(unhandled_exception_handler)
    PUSH_REGS 0
    CALL_HANDLER arm64_unhandled_exception_handler

    b       .
FUNCTION_END(unhandled_exception_handler)

PRIVATE_FUNCTION_START(sync_EL1h_handler)
    PUSH_REGS 0
    CALL_HANDLER arm64_sync_exception_handler
    POP_REGS 0

    eret
FUNCTION_END(sync_EL1h_handler)

PRIVATE_FUNCTION_START(irq_EL1h_handler)
    PUSH_REGS 0
    CALL_HANDLER arm64_irq_handler
    POP_REGS 0

    eret
FUNCTION_END(irq_EL1h_handler)

PRIVATE_FUNCTION_START(sync_EL0_64_handler)
    PUSH_REGS 1

    /* Check for SVC in ESR.EC. */
    mrs     x9, esr_el1
    lsr     x10, x9, #ARM64_ESR_EC_SHIFT
    cmp     x10, #21
    beq     .Lsyscall_handler

    CALL_HANDLER arm64_sync_exception_handler

.Lreturn:
    POP_REGS 1
    eret

.Lsyscall_handler:
    /* Get system call number from ESR and get the system call table entry.
     * Use callee-save registers to preserve across thread_at_kernel_entry().
     * x19 = frame (left by PUSH_REGS)
     * x20 = call number
     * x21 = syscall_table_size
     * x22 = &syscall_table[x20]
     * x23 = original SP */
    and     x20, x9, #ARM64_ESR_ISS_MASK
    mov     x22, xzr
    adr_l   x21, syscall_table_size
    ldr     x21, [x21]
    cmp     x20, x21
    bhi     1f          /* Leave syscall table entry pointer null if invalid number. */
    adr_l   x22, syscall_table
    add     x22, x22, x20, lsl #4
1:
    /* Perform kernel entry work. */
    mov     x0, x22
    bl      thread_at_kernel_entry

    /* Enable interrupts. */
    msr     daifclr, #2

    /* Preserve SP in case we need to modify it. */
    mov     x23, sp

    /* Check for invalid system call. */
    cmp     xzr, x22
    beq     .Linvalid_syscall

    /* Check for stack arguments. */
    ldr     x9, [x22,#8]
    cmp     x9, #8
    bgt     .Lstack_args

.Lsyscall_regs:
    /* Restore the arguments and call the function. */
    ldr     x0, [x19,#FRAME_OFF_x0]
    ldr     x1, [x19,#FRAME_OFF_x1]
    ldr     x2, [x19,#FRAME_OFF_x2]
    ldr     x3, [x19,#FRAME_OFF_x3]
    ldr     x4, [x19,#FRAME_OFF_x4]
    ldr     x5, [x19,#FRAME_OFF_x5]
    ldr     x6, [x19,#FRAME_OFF_x6]
    ldr     x7, [x19,#FRAME_OFF_x7]
    ldr     x9, [x22]
    blr     x9

.Lsyscall_return:
    /* Restore original SP. */
    mov     sp, x23

    /* Save the return value of the system call. */
    // TODO: kern_thread_restore().
    str     x0, [x19,#FRAME_OFF_x0]

    /* Disable interrupts. */
    msr     daifset, #2

    /* Perform kernel exit work. */
    mov     x1, x0
    mov     x0, x22
    bl      thread_at_kernel_exit

    b       .Lreturn

.Lstack_args:
    /* Work out how many bytes to copy and reserve stack space. */
    sub     x2, x9, #8
    lsl     x2, x2, #3
    sub     sp, sp, x2

    /* Copy arguments. Source is userspace stack address. */
    mov     x0, sp
    ldr     x1, [x19,#FRAME_OFF_sp]
    bl      memcpy_from_user
    cmp     x0, #STATUS_SUCCESS
    bne     .Lcopy_failed

    b       .Lsyscall_regs

.Linvalid_syscall:
    mov     x0, #STATUS_INVALID_SYSCALL
    b       .Lsyscall_return

.Lcopy_failed:
    mov     x0, #STATUS_INVALID_ADDR
    b       .Lsyscall_return
FUNCTION_END(sync_EL0_64_handler)

PRIVATE_FUNCTION_START(irq_EL0_64_handler)
    PUSH_REGS 1
    CALL_HANDLER arm64_irq_handler
    POP_REGS 1

    eret
FUNCTION_END(irq_EL0_64_handler)

/** Enter user mode in the current thread.
 * @param frame         Previously prepared frame. */
FUNCTION_START(arch_thread_user_enter)
    /* Point SP at the frame to restore. */
    mov     sp, x0

    /* Restore and return to EL0. */
    POP_REGS 1
    eret
FUNCTION_END(arch_thread_user_enter)

.p2align 12
FUNCTION_START(arm64_exception_vectors)
.Lsync_EL1t_handler:        /* Synchronous exception from EL1 using EL0 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lirq_EL1t_handler:         /* IRQ from EL1 using EL0 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lfiq_EL1t_handler:         /* FIQ from EL1 using EL0 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lserror_EL1t_handler:      /* SError from EL1 using EL0 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lsync_EL1h_handler:        /* Synchronous exception from EL1 using EL1 SP. */
    b       sync_EL1h_handler

.p2align 7
.Lirq_EL1h_handler:         /* IRQ from EL1 using EL1 SP. */
    b       irq_EL1h_handler

.p2align 7
.Lfiq_EL1h_handler:         /* FIQ from EL1 using EL1 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lserror_EL1h_handler:      /* SError from EL1 using EL1 SP. */
    b       unhandled_exception_handler

.p2align 7
.Lsync_EL0_64_handler:      /* Synchronous exception from EL0 64-bit. */
    b       sync_EL0_64_handler

.p2align 7
.Lirq_EL0_64_handler:       /* IRQ from EL0 64-bit. */
    b       irq_EL0_64_handler

.p2align 7
.Lfiq_EL0_64_handler:       /* FIQ from EL0 64-bit. */
    b       unhandled_exception_handler

.p2align 7
.Lserror_EL0_64_handler:    /* SError from EL0 64-bit. */
    b       unhandled_exception_handler

.p2align 7
.Lsync_EL0_32_handler:      /* Synchronous exception from EL0 32-bit. */
    b       unhandled_exception_handler

.p2align 7
.Lirq_EL0_32_handler:       /* IRQ from EL0 32-bit. */
    b       unhandled_exception_handler

.p2align 7
.Lfiq_EL0_32_handler:       /* FIQ from EL0 32-bit. */
    b       unhandled_exception_handler

.p2align 7
.Lserror_EL0_32_handler:    /* SError from EL0 32-bit. */
    b       unhandled_exception_handler
FUNCTION_END(arm64_exception_vectors)
