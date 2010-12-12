/*
 * Copyright (C) 2010 Alex Smith
 *
 * Kiwi is open source software, released under the terms of the Non-Profit
 * Open Software License 3.0. You should have received a copy of the
 * licensing information along with the source code distribution. If you
 * have not received a copy of the license, please refer to the Kiwi
 * project website.
 *
 * Please note that if you modify this file, the license requires you to
 * ADD your name to the list of contributors. This boilerplate is not the
 * license itself; please refer to the copy of the license you have received
 * for complete terms.
 */

/**
 * @file
 * @brief		x86 time handling functions.
 */

#include <cpu/cpu.h>
#include <time.h>

/** Value of TSC when time_arch_init() was called. */
static uint64_t boot_time_offset = 0;

/** Read the Time Stamp Counter.
 * @return		Value of the TSC. */
static inline uint64_t rdtsc(void) {
	uint32_t high, low;
	__asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
	return ((uint64_t)high << 32) | low;
}

/** Get the system time (number of microseconds since boot).
 * @return		Number of microseconds since system was booted. */
useconds_t system_time(void) {
	return (useconds_t)((rdtsc() - boot_time_offset) / curr_cpu->arch.cycles_per_us);
}

/** Set up the boot time offset. */
void __init_text time_arch_init(void) {
	/* Initialise the boot time offset. In system_time() this value is
	 * subtracted from the value returned from TSC. This is necessary
	 * because although the bootloader set the TSC to 0, QEMU (and
	 * possibly some other things) don't support writing the TSC. */
	boot_time_offset = rdtsc();
}
