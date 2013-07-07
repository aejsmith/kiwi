/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		PC RTC functions.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <sync/spinlock.h>

#include <time.h>

/** Converts a BCD value from the RTC to decimal. */
#define BCD2DEC(num)	((((num & 0xF0) >> 4) * 10) + (num & 0x0F))

/** Lock serialising accesses to the RTC. */
static SPINLOCK_DECLARE(rtc_lock);

/** Get the number of nanoseconds since the Epoch from the RTC.
 * @return		Number of nanoseconds since Epoch. */
nstime_t platform_time_from_hardware(void) {
	uint32_t year, month, day, hour, min, sec;
	uint8_t tmp;

	spinlock_lock(&rtc_lock);

	/* Check if an update is in progress. */
	out8(0x70, 0x0A);
	while(in8(0x71) & 0x80) {
		arch_cpu_spin_hint();
		out8(0x70, 0x0A);
	}

	/* Read in each value. */
	out8(0x70, 0x00);
	sec = BCD2DEC(in8(0x71));
	out8(0x70, 0x02);
	min = BCD2DEC(in8(0x71));
	out8(0x70, 0x07);
	day = BCD2DEC(in8(0x71));
	out8(0x70, 0x08);
	month = BCD2DEC(in8(0x71));

	/* Make a nice big assumption about which year we're in. */
	out8(0x70, 0x09);
	year = BCD2DEC(in8(0x71)) + 2000;

	/* Hours need special handling, we need to check whether they are in
	 * 12- or 24-hour mode. If the high bit is set, then it is in 12-hour
	 * mode, PM, meaning we must add 12 to it. */
	out8(0x70, 0x04);
	tmp = in8(0x71);
	if(tmp & (1<<7)) {
		hour = BCD2DEC(tmp) + 12;
	} else {
		hour = BCD2DEC(tmp);
	}

	spinlock_unlock(&rtc_lock);
	return time_to_unix(year, month, day, hour, min, sec);
}
