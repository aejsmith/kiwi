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
#include <mm/page.h>

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
	list_init(&cpu->header);
	cpu->id = args->id;

	/* Initialise architecture-specific data. */
	cpu_arch_init(cpu, &args->arch);

	/* Initialise IPI information. */
	list_init(&cpu->ipi_queue);
	spinlock_init(&cpu->ipi_lock, "ipi_lock");

	/* Initialise timer information. */
	list_init(&cpu->timers);
	spinlock_init(&cpu->timer_lock, "timer_lock");

	/* Store in the CPU array. */
	if(cpus) {
		cpus[cpu->id] = cpu;
	}
}

/** Register all non-boot CPUs.
 * @param args		Kernel arguments structure. */
void __init_text cpu_init(kernel_args_t *args) {
	kernel_args_cpu_t *cpu;
	phys_ptr_t addr;

	/* Create the CPU array and add the boot CPU to it. */
	cpus = kcalloc(cpu_id_max + 1, sizeof(cpu_t *), MM_FATAL);
	cpus[boot_cpu.id] = &boot_cpu;

	/* Add all non-boot CPUs. */
	for(addr = args->cpus; addr;) {
		cpu = page_phys_map(addr, sizeof(kernel_args_cpu_t), MM_FATAL);

		if(cpu->id != boot_cpu.id) {
			cpu_add(kmalloc(sizeof(cpu_t), MM_FATAL), cpu);
		}

		addr = cpu->next;
		page_phys_unmap(cpu, sizeof(kernel_args_cpu_t), false);
	}
}

/** Initialise the boot CPU structure and CPU pointer.
 * @param args		Kernel arguments structure. */
void __init_text cpu_early_init(kernel_args_t *args) {
	/* Store a few details. */
	cpu_id_max = args->highest_cpu_id;
	cpu_count = args->cpu_count;

	/* Add the boot CPU. */
	cpu_add(&boot_cpu, (kernel_args_cpu_t *)((ptr_t)args->cpus));
	list_append(&cpus_running, &boot_cpu.header);

	/* Set the CPU pointer. */
	cpu_set_pointer((ptr_t)&boot_cpu);
}
