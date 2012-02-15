/*
 * Copyright (C) 2010-2011 Alex Smith
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
 * @brief		AMD64 time handling functions.
 *
 * @todo		Handle systems where the TSC is not invariant. We
 *			should use the HPET or PIT on such systems.
 */

#include <x86/cpu.h>
#include <x86/tsc.h>

#include <cpu.h>
#include <kernel.h>
#include <smp.h>
#include <time.h>

/** SMP boot status values used for synchronisation. */
#define SMP_BOOT_TSC_SYNC1	4
#define SMP_BOOT_TSC_SYNC2	5

/** Boot CPU system_time() value. */
static useconds_t system_time_sync __init_data;

/** Get the system time (number of microseconds since boot).
 * @return		Number of microseconds since system was booted. */
useconds_t system_time(void) {
	return (useconds_t)((x86_rdtsc() - curr_cpu->arch.system_time_offset) / curr_cpu->arch.cycles_per_us);
}

/** Spin for a certain amount of time.
 * @param us		Microseconds to spin for. */
void spin(useconds_t us) {
        uint64_t target = x86_rdtsc() + (us * curr_cpu->arch.cycles_per_us);

        /* Spin until we reach the target. */
        while(x86_rdtsc() < target) {
                cpu_spin_hint();
        }
}

/** Set up the boot time offset. */
__init_text void tsc_init_target(void) {
	/* Calculate the offset to subtract from the TSC when calculating the
	 * system time. For the boot CPU, this is the current value of the TSC,
	 * so the system time at this point is 0. For other CPUs, we need to
	 * synchronise against the boot CPU so system_time() reads the same
	 * value on all CPUs. */
#if CONFIG_SMP
	if(curr_cpu == &boot_cpu) {
#endif
		curr_cpu->arch.system_time_offset = x86_rdtsc();
#if CONFIG_SMP
	} else {
		/* Tell the boot CPU that we're here. */
		smp_boot_status = SMP_BOOT_TSC_SYNC1;

		/* Wait for it to store its system_time() value. */
		while(smp_boot_status != SMP_BOOT_TSC_SYNC2) {
			cpu_spin_hint();
		}

		/* Calculate the offset we need to use. */
		curr_cpu->arch.system_time_offset = -((system_time_sync * curr_cpu->arch.cycles_per_us) - x86_rdtsc());
	}
#endif
}

#if CONFIG_SMP
/** Boot CPU side of TSC initialization. */
__init_text void tsc_init_source(void) {
	/* Wait for the AP to get into tsc_init_target(). */
	while(smp_boot_status != SMP_BOOT_TSC_SYNC1) {
		cpu_spin_hint();
	}

	/* Save our system_time() value. */
	system_time_sync = system_time();
	smp_boot_status = SMP_BOOT_TSC_SYNC2;
}
#endif
