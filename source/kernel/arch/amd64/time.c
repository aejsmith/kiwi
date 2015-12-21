/*
 * Copyright (C) 2010-2013 Alex Smith
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
 * @brief               AMD64 time handling functions.
 *
 * TODO:
 *  - Handle systems where the TSC is not invariant. We should use the HPET or
 *    PIT on such systems.
 *  - Because I'm lazy this is only microsecond resolution at the moment. Doing
 *    nanosecond resolution requires some fixed point maths fun. Something along
 *    the lines of:
 *        cv_factor = (cpu_freq << 32) / ns_per_sec;
 *        time = (tsc << 32) / ns_per_sec;
 *    The problem with this, however, is that you lose the top 32 bits of the
 *    TSC, which is really not very useful.
 */

#include <x86/cpu.h>
#include <x86/smp.h>
#include <x86/tsc.h>

#include <cpu.h>
#include <kernel.h>
#include <smp.h>
#include <time.h>

/** Boot CPU system_time() value. */
static volatile nstime_t system_time_sync __init_data;

/** Get the system time (number of nanoseconds since boot).
 * @return              Number of nanoseconds since system was booted. */
nstime_t system_time(void) {
    uint64_t usecs;

    preempt_disable();
    usecs = (x86_rdtsc() - curr_cpu->arch.system_time_offset) / curr_cpu->arch.cycles_per_us;
    preempt_enable();

    return usecs_to_nsecs(usecs);
}

/** Spin for a certain amount of time.
 * @param nsecs         Nanoseconds to spin for. */
void spin(nstime_t nsecs) {
    uint64_t usecs, start, target, current, cycles_per_us;
    cpu_id_t id;

    usecs = nsecs_to_usecs(nsecs);

    preempt_disable();

    while (true) {
        id = curr_cpu->id;
        cycles_per_us = curr_cpu->arch.cycles_per_us;
        start = x86_rdtsc();
        target = start + (usecs * cycles_per_us);

        while (true) {
            current = x86_rdtsc();
            if (current >= target) {
                preempt_enable();
                return;
            }

            preempt_enable();
            arch_cpu_spin_hint();
            preempt_disable();

            /* We may have been migrated to a different CPU. Recalculate the
             * target. This can lose accuracy, but we can only end up waiting
             * too long rather than not long enough. This is acceptable. */
            if (id != curr_cpu->id) {
                usecs -= (current - start) / cycles_per_us;
                break;
            }
        }
    }
}

/** Set up the boot time offset. */
__init_text void tsc_init_target(void) {
    /* Calculate the offset to subtract from the TSC when calculating the
     * system time. For the boot CPU, this is the current value of the TSC, so
     * the system time at this point is 0. For other CPUs, we need to
     * synchronise against the boot CPU so system_time() reads the same value
     * on all CPUs. */
    if (curr_cpu == &boot_cpu) {
        curr_cpu->arch.system_time_offset = x86_rdtsc();
    } else {
        /* Tell the boot CPU that we're here. */
        smp_boot_status = SMP_BOOT_TSC_SYNC1;

        /* Wait for it to store its system_time() value. */
        while (smp_boot_status != SMP_BOOT_TSC_SYNC2)
            arch_cpu_spin_hint();

        /* Calculate the offset we need to use. */
        curr_cpu->arch.system_time_offset =
            -((nsecs_to_usecs(system_time_sync) * curr_cpu->arch.cycles_per_us) - x86_rdtsc());
    }
}

/** Boot CPU side of TSC initialization. */
__init_text void tsc_init_source(void) {
    /* Wait for the AP to get into tsc_init_target(). */
    while (smp_boot_status != SMP_BOOT_TSC_SYNC1)
        arch_cpu_spin_hint();

    /* Save our system_time() value. */
    system_time_sync = system_time();
    smp_boot_status = SMP_BOOT_TSC_SYNC2;
}
