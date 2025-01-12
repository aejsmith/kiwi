/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64-specific thread definitions.
 */

#pragma once

#ifndef __ASM__

#include <types.h>

struct cpu;
struct frame;
struct thread;

/** x86-specific thread structure.
 * @note                The GS register is pointed to the copy of this structure
 *                      for the current thread. It is used to access per-CPU
 *                      data, and also to easily access per-thread data from
 *                      assembly code. If changing the layout of this structure,
 *                      be sure to updated the offset definitions below. */
typedef struct arch_thread {
    struct cpu *cpu;                /**< Current CPU pointer, for curr_cpu. */
    struct thread *parent;          /**< Pointer to containing thread, for curr_thread. */

    /** SYSCALL/SYSRET data. */
    ptr_t kernel_rsp;               /**< RSP for kernel entry via SYSCALL. */
    ptr_t user_rsp;                 /**< Temporary storage for user RSP. */

    /** Saved context switch stack pointer. */
    ptr_t saved_rsp;

    struct frame *user_frame;       /**< Frame from last user-mode entry. */
    unsigned long flags;            /**< Flags for the thread. */
    ptr_t tls_base;                 /**< TLS base address. */

    /** Number of consecutive runs that the FPU is used for. */
    unsigned fpu_count;

    /** FPU context save point. */
    char fpu[512] __aligned(16);
} __packed arch_thread_t;

/** Get the current thread structure pointer.
 * @return              Pointer to current thread structure. */
static inline struct thread *arch_curr_thread(void) {
    struct thread *addr;

    __asm__("mov %%gs:8, %0" : "=r"(addr));
    return addr;
}

#endif /* __ASM__ */

/** Flags for arch_thread_t. */
#define ARCH_THREAD_FRAME_MODIFIED  (1 << 0)    /**< Interrupt frame was modified. */
#define ARCH_THREAD_FRAME_RESTORED  (1 << 1)    /**< A pre-interrupt frame was restored. */
#define ARCH_THREAD_HAVE_FPU        (1 << 2)    /**< Thread has an FPU state saved. */
#define ARCH_THREAD_FREQUENT_FPU    (1 << 3)    /**< FPU is frequently used by the thread. */

/** Offsets in arch_thread_t. */
#define ARCH_THREAD_OFF_KERNEL_RSP  0x10
#define ARCH_THREAD_OFF_USER_RSP    0x18
#define ARCH_THREAD_OFF_USER_FRAME  0x28
#define ARCH_THREAD_OFF_FLAGS       0x30
