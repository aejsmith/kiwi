/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               CPU context structure.
 */

#pragma once

#include <kernel/types.h>

__KERNEL_EXTERN_C_BEGIN

#ifdef __x86_64__

/** Structure describing a CPU execution context. */
typedef struct cpu_context {
    unsigned long rax;
    unsigned long rbx;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rdi;
    unsigned long rsi;
    unsigned long rbp;
    unsigned long rsp;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
    unsigned long rflags;
    unsigned long rip;

    // TODO: FPU state...
} cpu_context_t;

#elif defined(__aarch64__)

/** Structure describing a CPU execution context. */
typedef struct cpu_context {
    unsigned long todo;
} cpu_context_t;

#else

#error "No cpu_context_t defined for this architecture"

#endif

__KERNEL_EXTERN_C_END
