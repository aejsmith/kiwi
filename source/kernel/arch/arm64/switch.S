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
 * @brief               ARM64 context switch function.
 */

#include <arm64/asm.h>

/** Perform a thread context switch.
 * @param %rdi          New stack pointer.
 * @param %rsi          Location to store old stack pointer at. */
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
