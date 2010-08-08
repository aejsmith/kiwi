/*
 * Copyright (C) 2009-2010 Alex Smith
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

#include <console.h>

/** Convert the serial port number from the configuration to a port number. */
#if CONFIG_PC_SERIAL_PORT == 1
# define SERIAL_PORT		0x3F8
#elif CONFIG_PC_SERIAL_PORT == 2
# define SERIAL_PORT		0x2F8
#elif CONFIG_PC_SERIAL_PORT == 3
# define SERIAL_PORT		0x3E8
#elif CONFIG_PC_SERIAL_PORT == 4
# define SERIAL_PORT		0x2E8
#endif

#endif /* __PLATFORM_CONSOLE_H */
