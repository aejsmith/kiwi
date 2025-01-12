/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 context switch function.
 */

#include <x86/asm.h>

/** Perform a thread context switch.
 * @param %rdi          New stack pointer.
 * @param %rsi          Location to store old stack pointer at. */
FUNCTION_START(amd64_context_switch)
    /* Just need to save callee-save registers: RBP, RBX, R12-15. */
    push    %rbp
    push    %rbx
    push    %r12
    push    %r13
    push    %r14
    push    %r15

    /* Save the current stack pointer. */
    movq    %rsp, (%rsi)
FUNCTION_START(amd64_context_restore)
    /* Set stack pointer. */
    movq    %rdi, %rsp

    /* Restore callee-save registers. */
    pop     %r15
    pop     %r14
    pop     %r13
    pop     %r12
    pop     %rbx
    pop     %rbp

    /* Stack pointer points at return address, just ret. */
    ret
FUNCTION_END(amd64_context_switch)
FUNCTION_END(amd64_context_restore)
