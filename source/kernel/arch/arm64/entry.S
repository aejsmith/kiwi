/*
 * Copyright (C) 2009-2021 Alex Smith
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

.p2align 12
SYMBOL(arm64_exception_vectors)
sync_EL1t_handler:          /* Synchronous exception from EL1 using EL0 SP. */
    b       .

.p2align 7
irq_EL1t_handler:           /* IRQ from EL1 using EL0 SP. */
    b       .

.p2align 7
fiq_EL1t_handler:           /* FIQ from EL1 using EL0 SP. */
    b       .

.p2align 7
serror_EL1t_handler:        /* SError from EL1 using EL0 SP. */
    b       .

.p2align 7
sync_EL1h_handler:          /* Synchronous exception from EL1 using EL1 SP. */
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
    add     x0, sp, #(30 * 8)
    stp     x30, x0, [sp,#-16]!

    mrs     x0, spsr_el1
    mrs     x1, elr_el1
    stp     x0, x1, [sp,#-16]!

    /* Pass frame to handler. */
    mov     x0, sp

    /* Set up stack frame with ELR as return address. */
    stp     x29, x1, [sp, #-16]!
    mov     x29, sp

    bl      arm64_sync_exception_handler

    b       .

.p2align 7
irq_EL1h_handler:           /* IRQ from EL1 using EL1 SP. */
    b       .

.p2align 7
fiq_EL1h_handler:           /* FIQ from EL1 using EL1 SP. */
    b       .

.p2align 7
serror_EL1h_handler:        /* SError from EL1 using EL1 SP. */
    b       .

.p2align 7
sync_EL0_64_handler:        /* Synchronous exception from EL0 64-bit. */
    b       .

.p2align 7
irq_EL0_64_handler:         /* IRQ from EL0 64-bit. */
    b       .

.p2align 7
fiq_EL0_64_handler:         /* FIQ from EL0 64-bit. */
    b       .

.p2align 7
serror_EL0_64_handler:      /* SError from EL0 64-bit. */
    b       .

.p2align 7
sync_EL0_32_handler:        /* Synchronous exception from EL0 32-bit. */
    b       .

.p2align 7
irq_EL0_32_handler:         /* IRQ from EL0 32-bit. */
    b       .

.p2align 7
fiq_EL0_32_handler:         /* FIQ from EL0 32-bit. */
    b       .

.p2align 7
serror_EL0_32_handler:      /* SError from EL0 32-bit. */
    b       .

/** Enter user mode in the current thread.
 * @param frame         Previously prepared frame. */
FUNCTION_START(arch_thread_user_enter)
    /* TODO. */
    mov     x0, #0
    str     x0, [x0]
    ret
FUNCTION_END(arch_thread_user_enter)
