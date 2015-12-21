/*
 * Copyright (C) 2008-2013 Alex Smith
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
 * @brief               AMD64 CPU management.
 */

#ifndef __ARCH_CPU_H
#define __ARCH_CPU_H

#include <x86/descriptor.h>

#include <types.h>

struct cpu;
struct thread;

/** Type used to store a CPU ID. */
typedef uint32_t cpu_id_t;

/** Architecture-specific CPU structure. */
typedef struct arch_cpu {
    /** Temporary per-CPU data used during initialization. */
    struct cpu *parent;                 /**< Current CPU pointer. */
    struct thread *thread;              /**< Current thread pointer. */

    /** Time conversion factors. */
    uint64_t cycles_per_us;             /**< CPU cycles per µs. */
    uint64_t lapic_timer_cv;            /**< LAPIC timer conversion factor. */
    int64_t system_time_offset;         /**< Value to subtract from TSC value for system_time(). */

    /** Per-CPU CPU structures. */
    gdt_entry_t gdt[GDT_ENTRY_COUNT];   /**< Array of GDT descriptors. */
    tss_t tss;                          /**< Task State Segment (TSS). */
    void *double_fault_stack;           /**< Pointer to the stack for double faults. */

    /** CPU information. */
    uint64_t cpu_freq;                  /**< CPU frequency in Hz. */
    uint64_t lapic_freq;                /**< LAPIC timer frequency in Hz. */
    char model_name[64];                /**< CPU model name. */
    uint8_t family;                     /**< CPU family. */
    uint8_t model;                      /**< CPU model. */
    uint8_t stepping;                   /**< CPU stepping. */
    int max_phys_bits;                  /**< Maximum physical address bits. */
    int max_virt_bits;                  /**< Maximum virtual address bits. */
    int cache_alignment;                /**< Cache line size. */
} arch_cpu_t;

/** Get the current CPU structure pointer.
 * @return              Pointer to current CPU structure. */
static inline struct cpu *arch_curr_cpu(void) {
    struct cpu *addr;

    __asm__("mov %%gs:0, %0" : "=r"(addr));
    return addr;
}

/** Halt the current CPU. */
static inline __noreturn void arch_cpu_halt(void) {
    while (true)
        __asm__ volatile("cli; hlt");
}

/** Place the CPU in an idle state until an interrupt occurs. */
static inline void arch_cpu_idle(void) {
    __asm__ volatile("sti; hlt; cli");
}

/** CPU-specific spin loop hint. */
static inline void arch_cpu_spin_hint(void) {
    /* See PAUSE instruction in Intel 64 and IA-32 Architectures Software
     * Developer's Manual, Volume 2B: Instruction Set Reference N-Z for
     * more information as to what this does. */
    __asm__ volatile("pause");
}

/** Invalidate CPU caches. */
static inline void arch_cpu_invalidate_caches(void) {
    __asm__ volatile("wbinvd");
}

#endif /* __ARCH_CPU_H */
