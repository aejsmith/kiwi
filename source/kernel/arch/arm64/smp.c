/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 SMP detection code.
 */
#include <kernel.h>
#include <smp.h>

/** Send an IPI interrupt to a single CPU.
 * @param dest          Destination CPU ID. */
void arch_smp_ipi(cpu_id_t dest) {
    fatal_todo();
}

/** Detect all secondary CPUs in the system. */
void arch_smp_detect(void) {
    /* TODO */
}

/** Prepare the SMP boot process. */
__init_text void arch_smp_boot_prepare(void) {
    /* TODO. */
}

/** Boot a secondary CPU.
 * @param cpu           CPU to boot. */
__init_text void arch_smp_boot(cpu_t *cpu) {
    fatal_todo();
}

/** Clean up after secondary CPUs have been booted. */
__init_text void arch_smp_boot_cleanup(void) {
    /* TODO. */
}
