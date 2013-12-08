/*
 * Copyright (C) 2009-2011 Alex Smith
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
 * @brief		CPU management.
 */

#ifndef __CPU_H
#define __CPU_H

#include <arch/cpu.h>
#include <lib/list.h>
#include <sync/spinlock.h>

struct sched_cpu;
struct smp_call;
struct thread;
struct vm_aspace;

/** Structure describing a CPU. */
typedef struct cpu {
	list_t header;			/**< Link to running CPUs list. */

	cpu_id_t id;			/**< ID of the CPU. */
	arch_cpu_t arch;		/**< Architecture-specific information. */

	/** Current state of the CPU. */
	enum {
		CPU_OFFLINE,		/**< Offline. */
		CPU_RUNNING,		/**< Running. */
	} state;

	/** Scheduler information. */
	struct sched_cpu *sched;	/**< Scheduler run queues/timers. */
	struct thread *thread;		/**< Currently executing thread. */
	struct vm_aspace *aspace;	/**< Address space currently in use. */
	bool should_preempt;		/**< Whether the CPU should be preempted. */
	bool idle;			/**< Whether the CPU is idle. */

	/** Timer information. */
	list_t timers;			/**< List of active timers. */
	bool timer_enabled;		/**< Whether the timer device is enabled. */
	spinlock_t timer_lock;		/**< Timer list lock. */

	#if CONFIG_SMP
	/** SMP call information. */
	list_t call_queue;		/**< List of calls queued to this CPU. */
	bool ipi_sent;			/**< Whether an IPI has been sent to the CPU. */
	struct smp_call *curr_call;	/**< SMP call currently being handled. */
	spinlock_t call_lock;		/**< Lock to protect call queue. */
	#endif
} cpu_t;

/**
 * Pointer to the CPU structure of the current CPU.
 *
 * This definition expands to a pointer to the CPU structure of the current
 * CPU. It should only be accessed in situations where the current thread
 * cannot be migrated to a different CPU, i.e. preemption or interrupts
 * disabled.
 */
#define curr_cpu	(arch_curr_cpu())

extern cpu_t boot_cpu;
extern size_t highest_cpu_id;
extern size_t cpu_count;
extern list_t running_cpus;
extern cpu_t **cpus;

#if CONFIG_SMP
extern cpu_t *cpu_register(cpu_id_t id, int state);
#endif
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

#endif /* __CPU_H */
