/* Kiwi test module
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
 * @brief		Test module.
 */

#include <console/kprintf.h>

#include <errors.h>
#include <module.h>

/** Initialization function for test module.
 * @return		0 on success, negative error code on failure. */
static int foo_init(void) {
	kprintf(LOG_NORMAL, "foo: test module is running!\n");
	return 0;
}

/** Unloading funtion for test module.
 * @return		0 on success, negative error code on failure. */
static int foo_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("foo");
MODULE_DESC("Test kernel module.");
MODULE_FUNCS(foo_init, foo_unload);
