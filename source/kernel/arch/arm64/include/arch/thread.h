/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
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

    struct frame *user_frame;       /**< Frame from last user-mode entry. */
} arch_thread_t;

/** Get the current thread structure pointer.
 * @return              Pointer to current thread structure. */
static inline struct thread *arch_curr_thread(void) {
    void **data;
    __asm__("mrs %0, tpidr_el1" : "=r"(data));
    return data[1];
}

#endif /* __ASM__ */

#define ARCH_THREAD_OFF_user_frame  24
