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

#include <arm64/asm.h>

.section .text, "ax", @progbits

.macro PUSH_REGS
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

    /* Restore original SP value and save it. */
    add     x0, sp, #(30 * 8)
    stp     x30, x0, [sp,#-16]!

    mrs     x0, spsr_el1
    mrs     x1, elr_el1
    stp     x0, x1, [sp,#-16]!

    /* Pass frame to handler. */
    mov     x0, sp

    /* Set up frame pointer (X29) with ELR as return address. */
    stp     x29, x1, [sp, #-16]!
    mov     x29, sp
.endm

.macro POP_REGS
    /* Frame pointer. */
    add     sp, sp, #16

    /* SPSR/ELR. */
    ldp     x0, x1, [sp], #16
    msr     spsr_el1, x0
    msr     elr_el1, x1

    /* X30/SP. SP doesn't need to be restored here, we'll get it back by the end. */
    ldp     x30, x0, [sp], #16

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
    PUSH_REGS

    bl      arm64_unhandled_exception_handler
    b       .
FUNCTION_END(unhandled_exception_handler)

PRIVATE_FUNCTION_START(sync_EL1h_handler)
    PUSH_REGS

    bl      arm64_sync_exception_handler

    POP_REGS
    eret
FUNCTION_END(sync_EL1h_handler)

PRIVATE_FUNCTION_START(irq_EL1h_handler)
    PUSH_REGS

    bl      arm64_irq_handler

    POP_REGS
    eret
FUNCTION_END(sync_EL1h_handler)

/** Enter user mode in the current thread.
 * @param frame         Previously prepared frame. */
FUNCTION_START(arch_thread_user_enter)
    /* TODO. */
    mov     x0, #0
    str     x0, [x0]
    ret
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
    b       unhandled_exception_handler

.p2align 7
.Lirq_EL0_64_handler:       /* IRQ from EL0 64-bit. */
    b       unhandled_exception_handler

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
