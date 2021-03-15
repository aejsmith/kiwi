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
