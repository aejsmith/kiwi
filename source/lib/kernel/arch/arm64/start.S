/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 kernel library startup code.
 */

#include <system/arm64/asm.h>

.section .text, "ax", @progbits

/** Kernel library entry point.
 * @param %rdi          Process argument block. */
FUNCTION_START(libkernel_start)
    /* Relocate ourself. */
    mov     x19, x0
    adrp    x1, _DYNAMIC
    add     x1, x1, #:lo12:_DYNAMIC
    bl      libkernel_relocate

    /* Call the main function. */
    mov     x0, x19
    b       libkernel_init
FUNCTION_END(libkernel_start)

.section ".note.GNU-stack", "", @progbits
