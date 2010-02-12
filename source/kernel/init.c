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
#include <kargs.h>
#include <version.h>

extern void kmain(kernel_args_t *args, uint32_t cpu);

extern initcall_t __initcall_start[];
extern initcall_t __initcall_end[];

/** Rendezvous variables for SMP boot. */
static atomic_t g_boot_rendezvous1 = 0;
static atomic_t g_boot_rendezvous2 = 0;
static atomic_t g_boot_rendezvous3 = 0;

/** Lock to serialise the early part of SMP boot. */
static SPINLOCK_DECLARE(g_boot_spinlock);

/** Wait until all CPUs reach a certain point.
 * @param args		Kernel arguments structure.
 * @param var		Variable to wait on. */
static inline void init_rendezvous(kernel_args_t *args, atomic_t *var) {
	atomic_inc(var);
	while(atomic_get(var) < (int)args->cpu_count);
}

/** Second-stage intialization thread.
 * @param _args		Kernel arguments structure pointer.
 * @param arg2		Thread argument (unused). */
static void init_thread(void *_args, void *arg2) {
	const char *pargs[] = { "/system/services/svcmgr", NULL }, *penv[] = { NULL };
	kernel_args_t *args = _args;
	initcall_t *initcall;
	int ret;

	/* Initialise other things. */
	vfs_init();

	/* Bring up secondary CPUs. The first rendezvous sets off their
	 * initialisation, the second waits for them to complete. */
	init_rendezvous(args, &g_boot_rendezvous2);
	init_rendezvous(args, &g_boot_rendezvous3);

	/* Call other initialisation functions. */
	for(initcall = __initcall_start; initcall != __initcall_end; initcall++) {
		(*initcall)();
	}

	console_update_boot_progress(10);

	/* Load boot-time modules and mount the root filesystem. */
	bootmod_load();

	/* Mount the root filesystem. */
	vfs_mount_root(args);

	/* Reclaim memory taken up by initialisation code/data. */
	page_late_init();

	console_update_boot_progress(100);

	/* Run the startup process. */
	if((ret = process_create(pargs, penv, PROCESS_CRITICAL, 0, PRIORITY_SYSTEM,
	                         kernel_proc, NULL)) != 0) {
		fatal("Could not create startup process (%d)", ret);
	}
}

/** Main function of the kernel.
 * @param args          Arguments from the bootloader.
 * @param cpu           CPU that the function is running on. */
void __init_text kmain(kernel_args_t *args, uint32_t cpu) {
	thread_t *thread;

	/* Wait for all CPUs to enter the kernel. */
	init_rendezvous(args, &g_boot_rendezvous1);

	if(cpu == args->boot_cpu) {
		cpu_early_init(args);
		console_early_init();

		/* Perform early architecture/platform initialisation. */
		arch_premm_init(args);
		platform_premm_init(args);

		/* Initialise memory management subsystems. */
		vmem_early_init();
		kheap_early_init();
		vmem_init();
		page_init(args);
		slab_init();
		kheap_init();
		malloc_init();
		vm_init();

		/* Set up the console. */
		console_init(args);
		kprintf(LOG_NORMAL, "kernel: version %s booting (%" PRIu32 " CPU(s))\n",
		        kiwi_ver_string, cpu_count);

		/* Perform second stage architecture/platform initialisation. */
		arch_postmm_init(args);
		platform_postmm_init(args);

		/* Register all other CPUs, and set up the IPI system. */
		cpu_init(args);
		ipi_init();

		/* Bring up the scheduler and friends. */
		process_init();
		thread_init();
		sched_init();
		thread_reaper_init();

		/* Now that the thread system is up and all CPUs have been
		 * regsitered, the slab allocator's reclaim thread can be
		 * started and the magazine layer can be enabled. */
		slab_late_init();

		/* Create the second stage initialisation thread. */
		if(thread_create("init", kernel_proc, 0, init_thread, args, NULL, &thread) != 0) {
			fatal("Could not create second-stage initialisation thread");
		}
		thread_run(thread);

		/* Finally begin executing other threads. */
		sched_enter();
	} else {
		/* Wait for the boot CPU to do its initialisation. */
		init_rendezvous(args, &g_boot_rendezvous2);

		spinlock_lock(&g_boot_spinlock, 0);

		/* Switch to the kernel page map, set the CPU pointer and do
		 * architecture/platform initialisation. */
		page_map_switch(&g_kernel_page_map);
		cpu_set_pointer((ptr_t)cpus[cpu]);
		arch_ap_init(args);
		platform_ap_init(args);

		/* We're running, add ourselves to the running CPU list. */
		list_append(&cpus_running, &curr_cpu->header);

		/* Do scheduler initialisation. */
		sched_init();

		spinlock_unlock(&g_boot_spinlock);

		/* Perform the final rendezvous and then enter the scheduler. */
		init_rendezvous(args, &g_boot_rendezvous3);
		sched_enter();
	}
}
