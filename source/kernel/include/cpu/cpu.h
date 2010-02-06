/*
 * Copyright (C) 2009 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		CPU management.
 */

#ifndef __CPU_CPU_H
#define __CPU_CPU_H

#include <arch/cpu.h>

#include <sync/spinlock.h>

#include <types/list.h>

struct vm_aspace;
struct sched_cpu;
struct thread;

/** Structure describing a CPU. */
typedef struct cpu {
	list_t header;			/**< Link to running CPUs list. */
	cpu_id_t id;			/**< ID of the CPU. */

	/** Current state of the CPU. */
	enum {
		CPU_DISABLED,		/**< CPU does not exist. */
		CPU_DOWN,		/**< CPU is not running. */
		CPU_RUNNING,		/**< CPU is running. */
	} state;

	cpu_arch_t arch;		/**< Architecture-specific information. */

	/** Scheduler information. */
	struct sched_cpu *sched;	/**< Scheduler run queues/timers. */
	struct thread *thread;		/**< Currently executing thread. */
	struct vm_aspace *aspace;	/**< Address space currently in use. */
	bool idle;			/**< Whether the CPU is idle. */

	/** IPI information. */
	list_t ipi_queue;		/**< List of IPI messages sent to this CPU. */
	bool ipi_sent;			/**< Whether it is necessary to send an IPI. */
	spinlock_t ipi_lock;		/**< Lock to protect IPI queue. */

	/** Timer information. */
	timeout_t tick_len;		/**< Current tick length. */
	list_t timer_list;		/**< List of active timers. */
	spinlock_t timer_lock;		/**< Timer list lock. */
} cpu_t;

/** Expands to a pointer to the CPU structure of the current CPU. */
#define curr_cpu	((cpu_t *)cpu_get_pointer())

extern size_t cpu_id_max;
extern size_t cpu_count;
extern list_t cpus_running;
extern cpu_t **cpus;

extern void cpu_pause_all(void);
extern void cpu_resume_all(void);
extern void cpu_halt_all(void);

extern void cpu_reschedule(cpu_t *cpu);

extern cpu_id_t cpu_current_id(void);

extern cpu_t *cpu_add(cpu_id_t id, int state);
extern void cpu_init(void);
extern void cpu_early_init(void);

extern int kdbg_cmd_cpus(int argc, char **argv);

#endif /* __CPU_CPU_H */
