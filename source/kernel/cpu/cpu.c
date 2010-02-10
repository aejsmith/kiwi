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
 *
 * Each CPU in the system is tracked by a cpu_t structure. This contains
 * information such as the CPU's ID, its current state, and its current thread.
 * Each kernel stack has a pointer to the CPU structure of the CPU it's being
 * used on at the bottom of it. The curr_cpu macro expands to the value of this
 * pointer, using cpu_get_pointer() to get its value.
 */

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <proc/sched.h>

#include <assert.h>
#include <console.h>
#include <fatal.h>
#include <kargs.h>

/** Boot CPU structure. */
static cpu_t boot_cpu;

/** Information about all CPUs. */
size_t cpu_id_max = 0;			/**< Highest CPU ID in the system. */
size_t cpu_count = 0;			/**< Number of CPUs. */
LIST_DECLARE(cpus_running);		/**< List of running CPUs. */
cpu_t **cpus = NULL;			/**< Array of CPU structure pointers (index == CPU ID). */

/** Initialise a CPU and add it to the list.
 * @param cpu		Structure to initialise.
 * @param args		Kernel arguments CPU structure. */
static void cpu_add(cpu_t *cpu, kernel_args_cpu_t *args) {
	memset(cpu, 0, sizeof(cpu_t));
	cpu->id = args->id;

	/* Initialise architecture-specific data. */
	cpu_arch_init(&cpu->arch, &args->arch);

	/* Initialise IPI information. */
	list_init(&cpu->ipi_queue);
	spinlock_init(&cpu->ipi_lock, "ipi_lock");

	/* Initialise timer information. */
	list_init(&cpu->timer_list);
	spinlock_init(&cpu->timer_lock, "timer_lock");
	cpu->tick_len = 0;

	/* Store in the list of running CPUs. */
	list_init(&cpu->header);
	list_append(&cpus_running, &cpu->header);
}
#if 0
/** Add a new CPU.
 *
 * Adds a new CPU to the CPU array.
 *
 * @param id		ID of the CPU.
 * @param state		Current state of the CPU.
 *
 * @return		Pointer to CPU structure.
 */
cpu_t *cpu_add(cpu_id_t id, int state) {
	assert(id != cpu_current_id());

	/* If the ID is higher than the maximum ID currently in the array,
	 * resize it. */
	if(id > cpu_id_max) {
		cpus = krealloc(cpus, sizeof(cpu_t *) * (id + 1), MM_FATAL);
		memset(&cpus[cpu_id_max + 1], 0, (id - cpu_id_max) * sizeof(cpu_t *));

		cpu_id_max = id;
	}

	/* Allocate a new CPU structure to track the CPU. */
	cpus[id] = kcalloc(1, sizeof(cpu_t), MM_FATAL);
	cpus[id]->id = id;
	cpus[id]->state = state;

	list_init(&cpus[id]->header);
	if(state == CPU_RUNNING) {
		list_append(&cpus_running, &cpus[id]->header);
	}

	/* Initialise IPI information. */
	list_init(&cpus[id]->ipi_queue);
	spinlock_init(&cpus[id]->ipi_lock, "ipi_lock");

	/* Initialise timer information. */
	list_init(&cpus[id]->timer_list);
	spinlock_init(&cpus[id]->timer_lock, "timer_lock");
	cpus[id]->tick_len = 0;

	cpu_count++;
	return cpus[id];
}

/** Properly initialise the CPU subsystem. */
void __init_text cpu_init(void) {
	/* First get the real ID of the boot CPU. */
	boot_cpu.id = cpu_id_max = cpu_current_id();
	cpu_count = 1;

	/* Now create the initial CPU array and add the boot CPU to it. */
	cpus = kcalloc(cpu_id_max + 1, sizeof(cpu_t *), MM_FATAL);
	cpus[boot_cpu.id] = &boot_cpu;
}
#endif
/** Initialise the boot CPU structure and CPU pointer.
 * @param args		Kernel arguments structure. */
void __init_text cpu_early_init(kernel_args_t *args) {
	/* Store a few details. */
	cpu_id_max = args->highest_cpu_id;
	cpu_count = args->cpu_count;

	/* Add the boot CPU. */
	cpu_add(&boot_cpu, (kernel_args_cpu_t *)((ptr_t)args->cpus));

	/* Set the CPU pointer. */
	cpu_set_pointer((ptr_t)&boot_cpu);
}
