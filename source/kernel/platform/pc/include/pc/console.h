/*
 * Copyright (C) 2009-2020 Alex Smith
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
 * @brief               PC console code.
 */

#pragma once

/** Convert the serial port number from the configuration to a port number. */
#if CONFIG_PC_SERIAL_PORT == 1
#   define SERIAL_PORT      0x3f8
#elif CONFIG_PC_SERIAL_PORT == 2
#   define SERIAL_PORT      0x2f8
#elif CONFIG_PC_SERIAL_PORT == 3
#   define SERIAL_PORT      0x3e8
#elif CONFIG_PC_SERIAL_PORT == 4
#   define SERIAL_PORT      0x2e8
#endif

/** VGA register definitions. */
#define VGA_AC_INDEX        0x3c0
#define VGA_AC_WRITE        0x3c0
#define VGA_AC_READ         0x3c1
#define VGA_MISC_WRITE      0x3c2
#define VGA_SEQ_INDEX       0x3c4
#define VGA_SEQ_DATA        0x3c5
#define VGA_DAC_READ_INDEX  0x3c7
#define VGA_DAC_WRITE_INDEX 0x3c8
#define VGA_DAC_DATA        0x3c9
#define VGA_MISC_READ       0x3cc
#define VGA_GC_INDEX        0x3ce
#define VGA_GC_DATA         0x3cf
#define VGA_CRTC_INDEX      0x3d4
#define VGA_CRTC_DATA       0x3d5
#define VGA_INSTAT_READ     0x3da
#define VGA_NUM_SEQ_REGS    5
#define VGA_NUM_CRTC_REGS   25
#define VGA_NUM_GC_REGS     9
#define VGA_NUM_AC_REGS     21
#define VGA_NUM_REGS        (1 + VGA_NUM_SEQ_REGS + VGA_NUM_CRTC_REGS + VGA_NUM_GC_REGS + VGA_NUM_AC_REGS)

/** Keyboard code definitions */
#define LEFT_CTRL           0x1d
#define RIGHT_CTRL          0x1d
#define LEFT_ALT            0x38
#define RIGHT_ALT           0x38
#define LEFT_SHIFT          0x2a
#define RIGHT_SHIFT         0x36

extern void i8042_init(void);
