/*
 * Copyright (C) 2009 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/page.h>

#include <proc/sched.h>

#include <assert.h>
#include <console.h>
#include <kargs.h>

/** Boot CPU structure. */
cpu_t boot_cpu;

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
		cpu = phys_map(addr, sizeof(kernel_args_cpu_t), MM_FATAL);

		if(cpu->id != boot_cpu.id) {
			cpu_add(kmalloc(sizeof(cpu_t), MM_FATAL), cpu);
		}

		addr = cpu->next;
		phys_unmap(cpu, sizeof(kernel_args_cpu_t), false);
	}
}

/** Initialise the boot CPU structure.
 * @param args		Kernel arguments structure. */
void __init_text cpu_early_init(kernel_args_t *args) {
	/* Store a few details from the kernel arguments. */
	cpu_id_max = args->highest_cpu_id;
	cpu_count = args->cpu_count;

	/* Add the boot CPU. */
	cpu_add(&boot_cpu, (kernel_args_cpu_t *)((ptr_t)args->cpus));
	list_append(&cpus_running, &boot_cpu.header);
}
