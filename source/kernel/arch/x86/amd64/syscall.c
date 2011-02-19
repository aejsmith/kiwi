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
 * @brief		AMD64 system call setup code.
 */

#include <arch/x86/cpu.h>
#include <arch/x86/descriptor.h>

#include <cpu/cpu.h>

#include <console.h>

extern void syscall_arch_init(void);
extern void syscall_entry(void);

/** Set up SYSCALL/SYSRET support for AMD64. */
void __init_text syscall_arch_init(void) {
	uint64_t fmask, lstar, star;

	/* Disable interrupts and clear direction flag upon entry. */
	fmask = X86_FLAGS_IF | X86_FLAGS_DF;

	/* Set system call entry address. */
	lstar = (uint64_t)syscall_entry;

	/* Set segments for entry and returning. In 64-bit mode things happen
	 * as follows upon entry:
	 *  - CS is set to the value in IA32_STAR[47:32].
	 *  - SS is set to the value in IA32_STAR[47:32] + 8.
	 * Upon return to 64-bit mode, the following happens:
	 *  - CS is set to (the value in IA32_STAR[63:48] + 16).
	 *  - SS is set to (the value in IA32_STAR[63:48] + 8).
	 * Weird. This means that we have to have a specific GDT order to
	 * make things work. We set the SYSRET values below to the kernel DS,
	 * so that we get the correct segment (kernel DS + 16 = user CS, and
	 * kernel DS + 8 = user DS). */
	star = ((uint64_t)(SEGMENT_K_DS | 0x03) << 48) | ((uint64_t)SEGMENT_K_CS << 32);

	/* Set System Call Enable (SCE) in EFER and write everything out. */
	x86_write_msr(X86_MSR_EFER, x86_read_msr(X86_MSR_EFER) | X86_EFER_SCE);
	x86_write_msr(X86_MSR_FMASK, fmask);
	x86_write_msr(X86_MSR_LSTAR, lstar);
	x86_write_msr(X86_MSR_STAR, star);

	kprintf(LOG_NORMAL, "syscall: set up SYSCALL MSRs on CPU %" PRIu32 ":\n", curr_cpu->id);
	kprintf(LOG_NORMAL, " FMASK: 0x%" PRIx64 "\n", fmask);
	kprintf(LOG_NORMAL, " LSTAR: 0x%" PRIx64 "\n", lstar);
	kprintf(LOG_NORMAL, " STAR:  0x%" PRIx64 "\n", star);
}
