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
 * @brief		Kernel console device request types.
 */

#ifndef __KERNEL_CONSOLE_H
#define __KERNEL_CONSOLE_H

/** Request types for the kernel console device. */
#define KCONSOLE_GET_LOG_SIZE		32	/**< Get the size of the log buffer. */
#define KCONSOLE_READ_LOG		33	/**< Read the log buffer. */
#define KCONSOLE_CLEAR_LOG		34	/**< Clear the log buffer. */
#define KCONSOLE_UPDATE_PROGRESS	35	/**< Update the boot progress bar. */

#endif /* __KERNEL_CONSOLE_H */
