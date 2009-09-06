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
#define VGA_CRTC_INDEX		0x3D4
#define VGA_CRTC_DATA		0x3D5

/** VGA console properties. */
#define VGA_CONSOLE_COLS	80	/**< Number of columns on the VGA console. */
#define VGA_CONSOLE_ROWS	25	/**< Number of rows on the VGA console. */
#define VGA_CONSOLE_FG		0xF	/**< Foreground colour (white). */
#define VGA_CONSOLE_BG		0x0	/**< Background colour (black). */

/** VGA attribute byte. */
#define VGA_ATTRIB		((uint16_t)((VGA_CONSOLE_BG << 4) | VGA_CONSOLE_FG) << 8)

extern void console_late_init(void);

#endif /* __PLATFORM_CONSOLE_H */
