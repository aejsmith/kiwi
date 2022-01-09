/*
 * Copyright (C) 2009-2022 Alex Smith
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
    __asm__ volatile("wfi");
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
