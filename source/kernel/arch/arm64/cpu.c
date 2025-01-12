/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 CPU management.
 */

#include <arm64/cpu.h>
#include <arm64/exception.h>
#include <arm64/time.h>

#include <cpu.h>
#include <kdb.h>
#include <kernel.h>
#include <smp.h>

/**
 * Get the current CPU ID.
 * 
 * Gets the ID of the CPU that the function executes on. This function should
 * only be used in cases where the curr_cpu variable is unavailable or unsafe.
 * Anywhere else you should be using curr_cpu->id.
 *
 * @return              Current CPU ID.
 */
cpu_id_t cpu_id(void) {
    /* TODO */
    return 0;
}

/** Dump information about a CPU.
 * @param cpu           CPU to dump. */
void cpu_dump(cpu_t *cpu) {
    kprintf(LOG_NOTICE, " cpu%" PRIu32 "\n", cpu->id);
    // TODO: Identification
}

/** Perform early initialization common to all CPUs. */
__init_text void arch_cpu_early_init(void) {
    /* Nothing happens. */
}

/** Detect and set up the current CPU.
 * @param cpu           CPU structure for the current CPU. */
__init_text void arch_cpu_early_init_percpu(cpu_t *cpu) {
    /* Set TPIDR_EL1 to point to the current CPU. This is what arch_curr_cpu()
     * uses, until we do the first thread switch, at which point arch_thread_t
     * takes over. */
    cpu->arch.parent = cpu;
    cpu->arch.thread = NULL;
    arm64_write_sysreg(tpidr_el1, &cpu->arch);
    arm64_isb();

    arm64_exception_init();
}

/** Display a list of running CPUs. */
static kdb_status_t kdb_cmd_cpus(int argc, char **argv, kdb_filter_t *filter) {
    if (kdb_help(argc, argv)) {
        kdb_printf("Usage: %s\n\n", argv[0]);

        kdb_printf("Prints a list of all CPUs and information about them.\n");
        return KDB_SUCCESS;
    }

    kdb_printf("TODO");

    return KDB_SUCCESS;
}

/** Perform additional initialization. */
__init_text void arch_cpu_init() {
    kdb_register_command("cpus", "Display a list of CPUs.", kdb_cmd_cpus);

    arm64_time_init();
}

/** Perform additional initialization of the current CPU. */
__init_text void arch_cpu_init_percpu() {
    /* TODO. */
}
