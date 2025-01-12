/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               CPU management.
 */

#pragma once

#include <arch/cpu.h>
#include <lib/list.h>
#include <sync/spinlock.h>

struct sched_cpu;
struct smp_call;
struct thread;
struct timer_thread;
struct vm_aspace;

/** Structure describing a CPU. */
typedef struct cpu {
    list_t header;                      /**< Link to running CPUs list. */

    cpu_id_t id;                        /**< ID of the CPU. */
    arch_cpu_t arch;                    /**< Architecture-specific information. */

    /** Current state of the CPU. */
    enum {
        CPU_OFFLINE,                    /**< Offline. */
        CPU_RUNNING,                    /**< Running. */
    } state;

    /** Scheduler information. */
    uint32_t in_interrupt;              /**< Count of nested interrupts. */
    struct sched_cpu *sched;            /**< Scheduler run queues/timers. */
    struct thread *thread;              /**< Currently executing thread. */
    struct vm_aspace *aspace;           /**< Address space currently in use. */
    bool should_preempt;                /**< Whether the CPU should be preempted. */
    bool idle;                          /**< Whether the CPU is idle. */

    /** Timer information. */
    list_t timers;                      /**< List of active timers. */
    bool timer_enabled;                 /**< Whether the timer device is enabled. */
    spinlock_t timer_lock;              /**< Timer list lock. */
    struct timer_thread *timer_thread;  /**< Timer thread. */

    /** SMP call information. */
    list_t call_queue;                  /**< List of calls queued to this CPU. */
    bool ipi_sent;                      /**< Whether an IPI has been sent to the CPU. */
    struct smp_call *curr_call;         /**< SMP call currently being handled. */
    spinlock_t call_lock;               /**< Lock to protect call queue. */
} cpu_t;

/**
 * Pointer to the CPU structure of the current CPU.
 *
 * This definition expands to a pointer to the CPU structure of the current
 * CPU. It should only be accessed in situations where the current thread
 * cannot be migrated to a different CPU, i.e. preemption or interrupts
 * disabled.
 */
#define curr_cpu        (arch_curr_cpu())

extern cpu_t boot_cpu;
extern size_t highest_cpu_id;
extern size_t cpu_count;
extern list_t running_cpus;
extern cpu_t **cpus;

extern bool in_interrupt(void);
extern void enter_interrupt(void);
extern void leave_interrupt(void);

extern cpu_t *cpu_register(cpu_id_t id, int state);

extern cpu_id_t cpu_id(void);
extern void cpu_dump(cpu_t *cpu);

extern void arch_cpu_early_init(void);
extern void arch_cpu_early_init_percpu(cpu_t *cpu);
extern void arch_cpu_init(void);
extern void arch_cpu_init_percpu(void);

extern void cpu_early_init(void);
extern void cpu_early_init_percpu(cpu_t *cpu);
extern void cpu_init(void);
extern void cpu_init_percpu(void);
