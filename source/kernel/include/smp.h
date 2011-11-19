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
 * @brief		Symmetric Multi-Processing (SMP) support.
 */

#ifndef __SMP_H
#define __SMP_H

#include <cpu.h>

#ifdef CONFIG_SMP

/** Flags to modify SMP call behaviour. */
#define SMP_CALL_ASYNC		(1<<0)	/**< Do not wait for target CPUs to complete call before returning. */

/** Type of an SMP call function.
 * @param arg		Argument to the call.
 * @return		Status code to return to the calling CPU. For broadcast
 *			or multicast calls, no value will be returned. */
typedef status_t (*smp_call_func_t)(void *arg);

extern volatile unsigned smp_boot_wait;

extern void arch_smp_ipi(cpu_id_t dest);

extern void smp_ipi_handler(void);

extern status_t smp_call_single(cpu_id_t dest, smp_call_func_t func, void *arg, unsigned flags);
extern void smp_call_broadcast(smp_call_func_t func, void *arg, unsigned flags);
//extern void smp_call_multicast(cpu_set_t *cpus, smp_call_func_t func, void *arg, unsigned flags);
extern void smp_call_acknowledge(status_t status);

extern void arch_smp_boot_prepare(void);
extern void arch_smp_boot(cpu_t *cpu);
extern void arch_smp_boot_cleanup(void);

extern void platform_smp_detect(void);

extern void smp_init(void);
extern void smp_boot(void);

#endif /* CONFIG_SMP */

#endif /* __SMP_H */
