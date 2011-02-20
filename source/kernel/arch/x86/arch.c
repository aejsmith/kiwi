/*
 * Copyright (C) 2009-2010 Alex Smith
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
 * @brief		x86 architecture core code.
 */

#include <x86/cpu.h>
#include <x86/lapic.h>
#include <x86/page.h>

#include <cpu/cpu.h>
#include <cpu/intr.h>

#include <console.h>
#include <kernel.h>
#include <time.h>

extern void syscall_arch_init(void);

/** x86-specific early initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_premm_init(kernel_args_t *args) {
	cpu_features_init(&cpu_features, args->arch.standard_ecx,
	                  args->arch.standard_edx, args->arch.extended_ecx,
	                  args->arch.extended_edx);
	descriptor_init(&boot_cpu);
	intr_init();
	pat_init();
}

/** x86-specific second stage initialisation.
 * @param args		Kernel arguments structure. */
void __init_text arch_postmm_init(kernel_args_t *args) {
#ifdef __x86_64__
	syscall_arch_init();
#else
	/* Set the correct CR3 value in the double fault TSS. When the TSS is
	 * set up by cpu_arch_init(), we are still on the PDP set up by the
	 * bootloader. */
	curr_cpu->arch.double_fault_tss.cr3 = x86_read_cr3();
#endif
	lapic_init(args);
}

/** x86-specific initialisation for an AP.
 * @param args		Kernel arguments structure.
 * @param cpu		CPU structure for the AP. */
void __init_text arch_ap_init(kernel_args_t *args, cpu_t *cpu) {
	descriptor_init(cpu);
	pat_init();
	lapic_init(args);
#ifdef __x86_64__
	syscall_arch_init();
#endif
}
