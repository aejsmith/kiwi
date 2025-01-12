/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               x86 TSC handling functions.
 */

#pragma once

/** Read the Time Stamp Counter.
 * @return              Value of the TSC. */
static inline uint64_t x86_rdtsc(void) {
    uint32_t high, low;

    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

extern void tsc_init_target(void);
extern void tsc_init_source(void);
