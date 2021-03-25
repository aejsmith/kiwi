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
 * @brief               ARM64-specific thread definitions.
 */

#pragma once

#ifndef __ASM__

#include <types.h>

struct cpu;
struct frame;
struct thread;

/** x86-specific thread structure. */
typedef struct arch_thread {
    /** Current CPU/thread information. TPIDR_EL1 points here. */
    struct cpu *cpu;                /**< Current CPU pointer, for curr_cpu. */
    struct thread *parent;          /**< Pointer to containing thread, for curr_thread. */

    /** Saved context switch stack pointer. */
    ptr_t saved_sp;
} arch_thread_t;

/** Get the current thread structure pointer.
 * @return              Pointer to current thread structure. */
static inline struct thread *arch_curr_thread(void) {
    void **data;
    __asm__("mrs %0, tpidr_el1" : "=r"(data));
    return data[1];
}

#endif /* __ASM__ */
