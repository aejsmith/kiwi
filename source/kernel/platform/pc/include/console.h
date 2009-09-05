/* Kiwi PC console code
 * Copyright (C) 2009 Alex Smith
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
 * @brief		PC console code.
 */

#ifndef __PLATFORM_CONSOLE_H
#define __PLATFORM_CONSOLE_H

#include <console/console.h>

/** Convert the serial port number from the configuration to a port number. */
#if CONFIG_X86_SERIAL_PORT == 1
# define SERIAL_PORT		0x3F8
#elif CONFIG_X86_SERIAL_PORT == 2
# define SERIAL_PORT		0x2F8
#elif CONFIG_X86_SERIAL_PORT == 3
# define SERIAL_PORT		0x3E8
#elif CONFIG_X86_SERIAL_PORT == 4
# define SERIAL_PORT		0x2E8
#endif

/** Physical address of VGA memory. */
#define VGA_MEM_PHYS		0xB8000

/** VGA register definitions. */
#define	VGA_AC_INDEX		0x3C0
#define	VGA_AC_WRITE		0x3C0
#define	VGA_AC_READ		0x3C1
#define	VGA_MISC_WRITE		0x3C2
#define VGA_SEQ_INDEX		0x3C4
#define VGA_SEQ_DATA		0x3C5
#define	VGA_DAC_READ_INDEX	0x3C7
#define	VGA_DAC_WRITE_INDEX	0x3C8
#define	VGA_DAC_DATA		0x3C9
#define	VGA_MISC_READ		0x3CC
#define VGA_GC_INDEX 		0x3CE
#define VGA_GC_DATA 		0x3CF
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5
#define	VGA_INSTAT_READ		0x3DA
#define	VGA_NUM_SEQ_REGS	5
#define	VGA_NUM_CRTC_REGS	25
#define	VGA_NUM_GC_REGS		9
#define	VGA_NUM_AC_REGS		21
#define	VGA_NUM_REGS		(1 + VGA_NUM_SEQ_REGS + VGA_NUM_CRTC_REGS + \
				 VGA_NUM_GC_REGS + VGA_NUM_AC_REGS)

/** VGA console properties. */
#define VGA_CONSOLE_COLS	80	/**< Number of columns on the VGA console. */
#define VGA_CONSOLE_ROWS	50	/**< Number of rows on the VGA console. */
#define VGA_CONSOLE_FG		0xF	/**< Foreground colour (white). */
#define VGA_CONSOLE_BG		0x1	/**< Background colour (blue). */

/** VGA attribute byte. */
#define VGA_ATTRIB		((uint16_t)((VGA_CONSOLE_BG << 4) | VGA_CONSOLE_FG) << 8)

extern void console_late_init(void);

#endif /* __PLATFORM_CONSOLE_H */
