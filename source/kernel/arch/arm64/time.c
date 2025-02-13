/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               ARM64 generic timer functions.
 */

#include <arm64/cpu.h>
#include <arm64/time.h>

#include <arch/cpu.h>

#include <kernel.h>
#include <time.h>

static uint64_t arm64_timer_freq;
static uint64_t arm64_boot_time;

/** Get the system time (number of nanoseconds since boot).
 * @return              Number of nanoseconds since system was booted. */
nstime_t system_time(void) {
    uint64_t ticks = arm64_read_sysreg(cntvct_el0);
    ticks -= arm64_boot_time;
    return time_from_ticks(ticks, arm64_timer_freq);
}

/** Spin for a certain amount of time.
 * @param nsecs         Nanoseconds to spin for. */
void spin(nstime_t nsecs) {
    uint64_t current = arm64_read_sysreg(cntvct_el0);
    uint64_t target  = current + time_to_ticks(nsecs, arm64_timer_freq);

    while (current < target) {
        arch_cpu_spin_hint();
        current = arm64_read_sysreg(cntvct_el0);
    }
}

/** Initialize the ARM generic timer. */
__init_text void arm64_time_init(void) {
    /* Get the system timer frequency. This should be initialized by firmware. */
    arm64_timer_freq = arm64_read_sysreg(cntfrq_el0);
    if (arm64_timer_freq == 0) {
        fatal("Timer frequency has not been initialized by firmware");
    } else if (arm64_timer_freq > UINT32_MAX) {
        /* time_from_ticks only supports 32-bit frequency. */
        fatal("Timer frequency is too high");
    }

    kprintf(
        LOG_NOTICE, "time: ARM generic timer frequency is %" PRIu64 "MHz\n",
        arm64_timer_freq / 1000000);

    /* Boot time, this is the base for system_time(). */
    arm64_boot_time = arm64_read_sysreg(cntvct_el0);
}

/** Get the number of nanoseconds since the Epoch from the RTC.
 * @return              Number of nanoseconds since Epoch. */
nstime_t arch_time_from_hardware(void) {
    /* TODO */
    return 0;
}
