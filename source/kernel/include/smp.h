/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               Symmetric Multi-Processing (SMP) support.
 */

#pragma once

#include <cpu.h>
#include <kernel.h>
#include <status.h>

/** Flags to modify SMP call behaviour. */
#define SMP_CALL_ASYNC      (1<<0)  /**< Do not wait for target CPUs to complete call before returning. */

/** Type of an SMP call function.
 * @param arg           Argument to the call.
 * @return              Status code to return to the calling CPU. For broadcast
 *                      or multicast calls, no value will be returned. */
typedef status_t (*smp_call_func_t)(void *arg);

extern volatile unsigned smp_boot_status;

extern void arch_smp_ipi(cpu_id_t dest);

extern void smp_ipi_handler(void);

extern status_t smp_call_single(cpu_id_t dest, smp_call_func_t func, void *arg, unsigned flags);
extern void smp_call_broadcast(smp_call_func_t func, void *arg, unsigned flags);
//extern void smp_call_multicast(cpu_set_t *cpus, smp_call_func_t func, void *arg, unsigned flags);
extern void smp_call_acknowledge(status_t status);

/** Values for smp_boot_status (arch can use anything > 3). */
#define SMP_BOOT_INIT           0   /**< Boot process has started. */
#define SMP_BOOT_ALIVE          1   /**< AP has reached kmain_secondary(). */
#define SMP_BOOT_BOOTED         2   /**< AP has completed kmain_secondary(). */
#define SMP_BOOT_COMPLETE       3   /**< All APs have been booted. */

extern void arch_smp_detect(void);
extern void arch_smp_boot_prepare(void);
extern void arch_smp_boot(cpu_t *cpu);
extern void arch_smp_boot_cleanup(void);

extern void smp_init(void);
extern void smp_boot(void);
