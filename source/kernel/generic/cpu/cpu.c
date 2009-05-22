/* Kiwi CPU management
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

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <lib/string.h>

#include <mm/malloc.h>
#include <mm/slab.h>

#include <proc/sched.h>

#include <assert.h>
#include <fatal.h>

/** Boot CPU structure. */
static cpu_t boot_cpu;

/** Information about all CPUs. */
size_t cpu_id_max = 0;			/**< Highest CPU ID in the system. */
size_t cpu_count = 0;			/**< Number of all CPUs. */
LIST_DECLARE(cpus_running);		/**< List of running CPUs. */
cpu_t **cpus = NULL;			/**< Array of CPU structure pointers (index == CPU ID). */

#if CONFIG_SMP
/** Variable used by an AP to signal that it has booted. */
atomic_t ap_boot_wait = 0;

extern bool cpu_ipi_schedule_handler(unative_t num, intr_frame_t *frame);

/** Handler for a reschedule IPI.
 * @param num		Interrupt vector number.
 * @param frame		Interrupt stack frame.
 * @return		Always returns false. */
bool cpu_ipi_schedule_handler(unative_t num, intr_frame_t *frame) {
	sched_yield();
	return false;
}

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

	if(id > cpu_id_max) {
		/* Resize the CPU array. */
		cpus = krealloc(cpus, sizeof(cpu_t *) * (id + 1), MM_FATAL);
		memset(&cpus[cpu_id_max + 1], 0, (id - cpu_id_max) * sizeof(cpu_t *));

		cpu_id_max = id;
	}

	cpus[id] = kcalloc(1, sizeof(cpu_t), MM_FATAL);
	cpus[id]->id = id;
	cpus[id]->state = state;

	list_init(&cpus[id]->header);
	if(state == CPU_RUNNING) {
		list_append(&cpus_running, &cpus[id]->header);
	}

	/* Initialize timer information. */
	list_init(&cpus[id]->timer_list);
	spinlock_init(&cpus[id]->timer_lock, "timer_lock");
	cpus[id]->tick_len = 0;

	cpu_count++;
	return cpus[id];
}

/** Boot all detected secondary CPUs. */
void cpu_boot_all(void) {
	size_t i;

	for(i = 0; i <= cpu_id_max; i++) {
		if(cpus[i]->state == CPU_DOWN) {
			cpu_boot(cpus[i]);
		}
	}
}
#endif /* CONFIG_SMP */

/** Properly initialize the CPU subsystem and detect secondary CPUs. */
void cpu_init(void) {
	/* First get the real ID of the boot CPU. */
	boot_cpu.id = cpu_id_max = cpu_current_id();
	cpu_count = 1;

	/* Now create the initial CPU array and add the boot CPU to it. */
	cpus = kcalloc(cpu_id_max + 1, sizeof(cpu_t *), MM_FATAL);
	cpus[boot_cpu.id] = &boot_cpu;
#if CONFIG_SMP
	/* Detect secondary CPUs. */
	cpu_detect();
#endif
	/* Now that we know the CPU count, we can enable the magazine layer
	 * in the slab allocator. */
	slab_enable_cpu_cache();
}

/** Set up the boot CPU structure and the current CPU pointer. */
void cpu_early_init(void) {
	/* Set to 0 until we know the real ID. */
	boot_cpu.id = 0;
	boot_cpu.state = CPU_RUNNING;

	/* Set the current CPU pointer on the initial kernel stack. */
	cpu_set_pointer((ptr_t)&boot_cpu);

	list_init(&boot_cpu.header);
	list_append(&cpus_running, &boot_cpu.header);

	list_init(&boot_cpu.timer_list);
	spinlock_init(&boot_cpu.timer_lock, "timer_lock");
	boot_cpu.tick_len = 0;
}
