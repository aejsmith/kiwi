/* Kiwi POSIX subsystem kernel-mode component
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
 * @brief		POSIX subsystem kernel-mode component.
 */

#include <console/kprintf.h>

#include <errors.h>
#include <fatal.h>
#include <module.h>

/** POSIX module initialization function.
 * @return		0 on success, negative error code on failure. */
static int posix_init(void) {
	return 0;
}

MODULE_NAME("posix");
MODULE_DESC("POSIX subsystem kernel-mode component.");
MODULE_FUNCS(posix_init, NULL);
MODULE_DEPS("loader", "vfs");
