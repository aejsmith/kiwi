/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 context switch function.
 */

#include <arm64/asm.h>

/** Perform a thread context switch.
 * @param %x0           New stack pointer.
 * @param %x1          Location to store old stack pointer at. */
FUNCTION_START(arm64_context_switch)
    /* Just need to save callee-save registers. */
    stp     x29, x30, [sp,#-16]! /* LR/FP. */
    stp     x27, x28, [sp,#-16]!
    stp     x25, x26, [sp,#-16]!
    stp     x23, x24, [sp,#-16]!
    stp     x21, x22, [sp,#-16]!
    stp     x19, x20, [sp,#-16]!

    /* Save the current stack pointer. */
    mov     x9, sp
    str     x9, [x1]
FUNCTION_START(arm64_context_restore)
    /* Get new stack pointer. */
    mov     sp, x0

    /* Restore callee-save registers. */
    ldp     x19, x20, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x29, x30, [sp], #16

    /* Restored LR now points at return address, return. */
    ret
FUNCTION_END(arm64_context_switch)
FUNCTION_END(arm64_context_restore)
