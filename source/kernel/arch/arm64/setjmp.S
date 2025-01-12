/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 non-local jump functions.
 */

#include <arm64/asm.h>

FUNCTION_START(setjmp)
    /* Callee-saved registers. */
    stp     x19, x20, [x0,#0]
    stp     x21, x22, [x0,#16]
    stp     x23, x24, [x0,#32]
    stp     x25, x26, [x0,#48]
    stp     x27, x28, [x0,#64]

    /* FP and LR. */
    stp     x29, x30, [x0,#80]

    /* SP. */
    mov     x9, sp
    str     x9, [x0,#96]

    /* Return 0 from setjmp(). */
    mov     w0, #0
    ret
FUNCTION_END(setjmp)

FUNCTION_START(longjmp)
    /* Callee-saved registers. */
    ldp     x19, x20, [x0,#0]
    ldp     x21, x22, [x0,#16]
    ldp     x23, x24, [x0,#32]
    ldp     x25, x26, [x0,#48]
    ldp     x27, x28, [x0,#64]

    /* FP and LR. */
    ldp     x29, x30, [x0,#80]

    /* SP. */
    ldr     x9, [x0,#96]
    mov     sp, x9

    /* Cause setjmp() to return the specified value (change 0 to 1 if given). */
    cmp     w1, #0
    csinc   w0, w1, wzr, ne
    ret
FUNCTION_END(longjmp)
