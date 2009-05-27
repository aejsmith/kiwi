/* Kiwi kernel initialization code
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
 * @brief		Kernel initialization code.
 */

#include <arch/arch.h>

#include <console/kprintf.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>
#include <cpu/ipi.h>
#include <cpu/smp.h>

#include <mm/aspace.h>
#include <mm/kheap.h>
#include <mm/malloc.h>
#include <mm/page.h>
#include <mm/pmm.h>
#include <mm/slab.h>

#include <platform/platform.h>

#include <proc/process.h>
#include <proc/sched.h>
#include <proc/thread.h>

#include <time/timer.h>

#include <fatal.h>
#include <version.h>

extern void kmain_bsp(void *data);
extern void kmain_ap(void);

static int ipi_func(void *msg, unative_t data1, unative_t data2, unative_t data3, unative_t data4) {
	kprintf(LOG_NORMAL, "Receiver %u! %u %u %u %u\n", curr_cpu->id, data1, data2, data3, data4);
	return 42;
}

/** Second-stage intialization thread.
 * @param arg		Architecture initialization data. */
static void kinit_thread(void *data) {
	uint64_t count = 0;

	/* Bring up secondary CPUs. */
	smp_boot_cpus();

	/* Reclaim memory taken up by temporary initialization code/data. */
	pmm_init_reclaim();

	int ret = ipi_send(3, ipi_func, 1, 3, 3, 7, IPI_SEND_SYNC);
	kprintf(LOG_NORMAL, "Sender %u! %d\n", curr_cpu->id, ret);

	while(true) {
		timer_sleep(1);
		count++;
		kprintf(LOG_NORMAL, "%" PRIu64 " second(s)...\n", count);
	}
}

/** Kernel initialization function.
 *
 * Kernel initialization function for the boot CPU.
 *
 * @param data		Data to pass into the architecture setup code.
 */
void kmain_bsp(void *data) {
	thread_t *thread;

	cpu_early_init();
	console_early_init();

	kprintf(LOG_NORMAL, "\nKiwi v%s (%s) - built for %s-%s\n",
	        kiwi_ver_string, kiwi_ver_codename, CONFIG_ARCH, CONFIG_PLATFORM);
	kprintf(LOG_NORMAL, "Copyright (C) 2007-2009 Kiwi Developers\n\n");

	/* Perform early architecture/platform initialization. */
	arch_premm_init(data);
	platform_premm_init(data);

	/* Initialize all of the other memory management subsystems. */
	vmem_early_init();
	kheap_early_init();
	vmem_init();
	pmm_init();
	page_init();
	slab_init();
	kheap_init();
	malloc_init();
	aspace_init();

	/* Perform second stage architecture/platform initialization. */
	arch_postmm_init();
	platform_postmm_init();

	/* Detect secondary CPUs. */
	cpu_init();
	smp_detect_cpus();
	ipi_init();

	/* Now that we know the CPU count, we can enable the magazine layer
	 * in the slab allocator. */
	slab_enable_cpu_cache();

	/* Bring up the scheduler and friends. */
	process_init();
	thread_init();
	sched_init();

	/* Perform final architecture initialization. */
	arch_final_init();

	/* Create the second stage initialization thread. */
	if(thread_create("kinit", kernel_proc, 0, kinit_thread, data, &thread) != 0) {
		fatal("Could not create initialization thread");
	}
	thread_run(thread);

	/* We now become the boot CPU's idle thread. */
	sched_idle();
}

/** AP kernel initialization function. */
void kmain_ap(void) {
	curr_cpu->state = CPU_RUNNING;
	list_append(&cpus_running, &curr_cpu->header);

	arch_ap_init();
	platform_ap_init();
	sched_init();

	atomic_set(&ap_boot_wait, 1);

	/* We now become this CPU's idle thread. */
	sched_idle();
}
