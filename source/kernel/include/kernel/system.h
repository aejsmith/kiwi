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
 * @brief		Miscellaneous system functions.
 */

#ifndef __KERNEL_SYSTEM_H
#define __KERNEL_SYSTEM_H

#include <kernel/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Actions for system_shutdown(). */
#define SHUTDOWN_REBOOT		1	/**< Reboot the system. */
#define SHUTDOWN_POWEROFF	2	/**< Power off the system. */

extern status_t SYSCALL(system_shutdown)(int action);
extern void SYSCALL(system_fatal)(const char *message);

#ifdef __cplusplus
}
#endif

#endif /* __KERNEL_SYSTEM_H */
