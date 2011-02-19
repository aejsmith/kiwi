/*
 * Copyright (C) 2010 Alex Smith
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
