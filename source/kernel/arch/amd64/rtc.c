/*
 * SPDX-FileCopyrightText: (C) Alex Smith <alex@alex-smith.me.uk>
 * SPDX-License-Identifier: ISC
 */

/**
 * @file
 * @brief               AMD64 RTC functions.
 */

#include <arch/cpu.h>
#include <arch/io.h>

#include <sync/spinlock.h>

#include <time.h>

static SPINLOCK_DEFINE(rtc_lock);

static inline uint32_t bcd_to_dec(uint8_t num) {
    return (((num & 0xf0) >> 4) * 10) + (num & 0x0f);
}

/** Get the number of nanoseconds since the Epoch from the RTC.
 * @return              Number of nanoseconds since Epoch. */
nstime_t arch_time_from_hardware(void) {
    spinlock_lock(&rtc_lock);

    /* Check if an update is in progress. */
    out8(0x70, 0x0a);
    while (in8(0x71) & 0x80)
        out8(0x70, 0x0a);

    /* Read in each value. */
    out8(0x70, 0x00);
    uint32_t sec = bcd_to_dec(in8(0x71));
    out8(0x70, 0x02);
    uint32_t min = bcd_to_dec(in8(0x71));
    out8(0x70, 0x07);
    uint32_t day = bcd_to_dec(in8(0x71));
    out8(0x70, 0x08);
    uint32_t month = bcd_to_dec(in8(0x71));

    /* Make a nice big assumption about which year we're in. */
    out8(0x70, 0x09);
    uint32_t year = bcd_to_dec(in8(0x71)) + 2000;

    /* Hours need special handling, we need to check whether they are in 12- or
     * 24-hour mode. If the high bit is set, then it is in 12-hour mode, PM,
     * meaning we must add 12 to it. */
    out8(0x70, 0x04);
    uint8_t tmp   = in8(0x71);
    uint32_t hour = bcd_to_dec(tmp) + ((tmp & (1 << 7)) ? 12 : 0);

    spinlock_unlock(&rtc_lock);
    return time_to_unix(year, month, day, hour, min, sec);
}
