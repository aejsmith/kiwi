/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 kernel library startup code.
 */

#include <system/amd64/asm.h>

.section .text, "ax", @progbits

/** Kernel library entry point.
 * @param %rdi          Process argument block. */
FUNCTION_START(libkernel_start)
    /* Relocate ourself. */
    pushq   %rdi
    leaq    _DYNAMIC(%rip), %rsi
    call    libkernel_relocate

    /* Call the main function. */
    popq    %rdi
    jmp     libkernel_init
FUNCTION_END(libkernel_start)

.section ".note.GNU-stack", "", @progbits
