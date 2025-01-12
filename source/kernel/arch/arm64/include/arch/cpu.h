/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 CPU management.
 */

#pragma once

#include <types.h>

struct cpu;
struct thread;

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Architecture-specific CPU structure. */
typedef struct arch_cpu {
    /** Temporary per-CPU data (TPIDR) used before scheduler init. */
    struct cpu *parent;                 /**< Current CPU pointer. */
    struct thread *thread;              /**< Current thread pointer. */
} arch_cpu_t;

/** Get the current CPU structure pointer.
 * @return              Pointer to current CPU structure. */
static inline struct cpu *arch_curr_cpu(void) {
    void **data;
    __asm__("mrs %0, tpidr_el1" : "=r"(data));
    return data[0];
}

/** Get the current CPU structure pointer (volatile, forces compiler to reload).
 * @return              Pointer to current CPU structure. */
static inline struct cpu *arch_curr_cpu_volatile(void) {
    void **data;
    __asm__ __volatile__("mrs %0, tpidr_el1" : "=r"(data));
    return data[0];
}

/** Halt the current CPU. */
static inline __noreturn void arch_cpu_halt(void) {
    while (true) {
        __asm__ volatile("wfi");
    }
}

/** Place the CPU in an idle state until an interrupt occurs. */
static inline void arch_cpu_idle(void) {
    __asm__ volatile("msr daifclr, #2; wfi; msr daifset, #2");
}

/** CPU-specific spin loop hint. */
static inline void arch_cpu_spin_hint(void) {
    /* See PAUSE instruction in Intel 64 and IA-32 Architectures Software
     * Developer's Manual, Volume 2B: Instruction Set Reference N-Z for
     * more information as to what this does. */
    __asm__ volatile("yield");
}

/** Invalidate CPU caches. */
static inline void arch_cpu_invalidate_caches(void) {
    // TODO...
}
