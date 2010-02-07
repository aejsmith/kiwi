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
 * @brief		Kernel initialisation functions.
 */

#include <arch/arch.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>
#include <cpu/smp.h>

#include <io/vfs.h>

#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/slab.h>
#include <mm/vm.h>

#include <platform/platform.h>

#include <proc/handle.h>
#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <bootmod.h>
#include <console.h>
#include <fatal.h>
#include <init.h>
#include <version.h>

extern void init_bsp(void *data);
extern void init_ap(void);

extern initcall_t __initcall_start[];
extern initcall_t __initcall_end[];

/** Second-stage intialization thread.
 * @param arg1		Thread argument (unused).
 * @param arg2		Thread argument (unused). */
static void init_thread(void *arg1, void *arg2) {
	const char *args[] = { "/system/services/svcmgr", NULL }, *env[] = { NULL };
	initcall_t *initcall;
	int ret;

	/* Bring up secondary CPUs. */
	smp_boot_cpus();

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	/* Load boot-time modules and mount the root filesystem. */
	bootmod_load();
	vfs_late_init();

	/* Reclaim memory taken up by initialisation code/data. */
	page_init_reclaim();

	/* Run the startup process. */
	if((ret = process_create(args, env, PROCESS_CRITICAL, 0, PRIORITY_SYSTEM, kernel_proc, NULL)) != 0) {
		fatal("Could not create startup process (%d)", ret);
	}
}

/** Kernel initialisation function.
 *
 * Kernel initialisation function for the boot CPU.
 *
 * @param data		Data to pass into the architecture setup code.
 */
void init_bsp(void *data) {
	thread_t *thread;

	cpu_early_init();
	console_early_init();

	kprintf(LOG_NORMAL, "\nKiwi v%s - built for %s-%s\n",
	        kiwi_ver_string, CONFIG_ARCH, CONFIG_PLATFORM);
	kprintf(LOG_NORMAL, "Copyright (C) 2007-2009 Kiwi Developers\n\n");

	/* Perform early architecture/platform initialisation. */
	arch_premm_init(data);
	platform_premm_init(data);

	/* Initialise other memory management subsystems. */
	vmem_early_init();
	kheap_early_init();
	vmem_init();
	page_init();
	slab_init();
	kheap_init();
	malloc_init();
	vm_init();

	/* Perform second stage architecture/platform initialisation. */
	arch_postmm_init();
	platform_postmm_init();

	/* Detect secondary CPUs. */
	cpu_init();
	smp_detect_cpus();
	ipi_init();

	/* Bring up the scheduler and friends. */
	process_init();
	thread_init();
	sched_init();
	thread_reaper_init();

	/* Now that we know the CPU count and the thread system is up, we can
	 * enable the magazine layer in the slab allocator and start up its
	 * reclaim thread. */
	slab_late_init();

	/* Initialise other things. */
	vfs_init();

	/* Perform final architecture/platform initialisation. */
	platform_final_init();
	arch_final_init();

	/* Create the second stage initialisation thread. */
	if(thread_create("init", kernel_proc, 0, init_thread, NULL, NULL, &thread) != 0) {
		fatal("Could not create second-stage initialisation thread");
	}
	thread_run(thread);

	/* We now become the boot CPU's idle thread. */
	sched_idle();
}

/** AP kernel initialisation function. */
void init_ap(void) {
	curr_cpu->state = CPU_RUNNING;
	list_append(&cpus_running, &curr_cpu->header);

	arch_ap_init();
	platform_ap_init();
	sched_init();

	atomic_set(&ap_boot_wait, 1);

	/* We now become this CPU's idle thread. */
	sched_idle();
}
