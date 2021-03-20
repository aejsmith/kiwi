/*
 * Copyright (C) 2009-2021 Alex Smith
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
 * @brief               ARM64 CPU management.
 */

#include <arm64/exception.h>

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
    /* TODO. */
}

/** Send an IPI interrupt to a single CPU.
 * @param dest          Destination CPU ID. */
void arch_smp_ipi(cpu_id_t dest) {
    fatal_todo();
}

/** Perform early initialization common to all CPUs. */
__init_text void arch_cpu_early_init(void) {
    /* Nothing happens. */
}

/** Detect and set up the current CPU.
 * @param cpu           CPU structure for the current CPU. */
__init_text void arch_cpu_early_init_percpu(cpu_t *cpu) {
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

    /* TODO. */
}

/** Perform additional initialization of the current CPU. */
__init_text void arch_cpu_init_percpu() {
    /* TODO. */
}
