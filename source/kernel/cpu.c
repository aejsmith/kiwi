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
 *
 * Each CPU in the system is tracked by a cpu_t structure. This contains
 * information such as the CPU's ID, its current state, and its current
 * thread. An architecture-specific method is used to store a pointer to
 * the current CPU's structure, and the curr_cpu macro expands to the
 * value of this pointer.
 */

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <assert.h>
#include <cpu.h>
#include <kernel.h>

/** Boot CPU structure. */
cpu_t boot_cpu;

/** Information about all CPUs. */
size_t highest_cpu_id = 0;		/**< Highest CPU ID in the system. */
size_t cpu_count = 0;			/**< Number of CPUs. */
LIST_DECLARE(running_cpus);		/**< List of running CPUs. */
cpu_t **cpus = NULL;			/**< Array of CPU structure pointers (index == CPU ID). */

#if CONFIG_SMP
/** Variable to wait on while waiting for a CPU to boot. */
volatile int cpu_boot_wait = 0;
#endif

/** Initialize a CPU structure.
 * @param cpu		Structure to initialize.
 * @param id		ID of the CPU to add.
 * @param state		State of the CPU. */
static void cpu_ctor(cpu_t *cpu, cpu_id_t id, int state) {
	memset(cpu, 0, sizeof(cpu_t));
	list_init(&cpu->header);
	cpu->id = id;
	cpu->state = state;

	#if CONFIG_SMP
	/* Initialize SMP call information. */
	list_init(&cpu->call_queue);
	spinlock_init(&cpu->call_lock, "ipi_lock");
	#endif

	/* Initialize timer information. */
	list_init(&cpu->timers);
	spinlock_init(&cpu->timer_lock, "timer_lock");
}

#if CONFIG_SMP
/** Register a non-boot CPU.
 * @param id		ID of CPU to add.
 * @param state		Current state of the CPU.
 * @return		Pointer to CPU structure. */
cpu_t *cpu_register(cpu_id_t id, int state) {
	cpu_t *cpu;

	assert(cpus);

	cpu = kmalloc(sizeof(*cpu), MM_BOOT);
	cpu_ctor(cpu, id, state);

	/* Resize the CPU array if required. */
	if(id > highest_cpu_id) {
		cpus = krealloc(cpus, sizeof(cpu_t *) * (id + 1), MM_BOOT);
		memset(&cpus[highest_cpu_id + 1], 0, (id - highest_cpu_id) * sizeof(cpu_t *));

                highest_cpu_id = id;
        }

	assert(!cpus[id]);
	cpus[id] = cpu;
	cpu_count++;
	return cpu;
}
#endif

/** Perform early CPU subsystem initialization. */
__init_text void cpu_early_init(void) {
	/* The boot CPU is initially assigned an ID of 0. It is later corrected
	 * once we have the ability to get the real ID. */
	cpu_ctor(&boot_cpu, 0, CPU_RUNNING);

	/* Perform architecture initialization. This initializes some state
	 * shared between all CPUs. */
	arch_cpu_early_init();

	/* We're being called on the boot CPU, initialize that. */
	cpu_early_init_percpu(&boot_cpu);
}

/** Perform early per-CPU initialization.
 * @param cpu		Structure for the current CPU. */
__init_text void cpu_early_init_percpu(cpu_t *cpu) {
	arch_cpu_early_init_percpu(cpu);

	/* Add ourself to the running CPU list. */
	cpu->state = CPU_RUNNING;
	list_append(&running_cpus, &curr_cpu->header);
}

/** Properly initialize the CPU subsystem. */
__init_text void cpu_init(void) {
	/* Get the real ID of the boot CPU. */
	boot_cpu.id = highest_cpu_id = cpu_id();
	cpu_count = 1;

	/* Create the initial CPU array and add the boot CPU to it. */
	cpus = kcalloc(highest_cpu_id + 1, sizeof(cpu_t *), MM_BOOT);
	cpus[boot_cpu.id] = &boot_cpu;

	/* We are called on the boot CPU. */
	cpu_init_percpu();
}

/** Perform additional per-CPU initialization. */
__init_text void cpu_init_percpu(void) {
	arch_cpu_init_percpu();
}
