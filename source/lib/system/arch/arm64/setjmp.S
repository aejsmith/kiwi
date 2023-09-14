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
 * @brief               ARM64 non-local jump functions.
 */

#include <system/arm64/asm.h>

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
