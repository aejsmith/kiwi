/* Kiwi virtual filesystem
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
 * @brief		Virtual filesystem.
 */

#include <errors.h>
#include <init.h>
#include <module.h>

#include "vfs_priv.h"

/** Initialization callback to mount the root filesystem. */
static CALLBACK_DECLARE(vfs_mount_root_callback, vfs_mount_root, NULL);

/** Initialization function for VFS.
 * @return		0 on success, negative error code on failure. */
static int vfs_init(void) {
	/* Initialize the node slab cache. */
	vfs_node_cache_init();

	/* Add an initialization callback function. */
	callback_add(&init_completion_cb_list, &vfs_mount_root_callback);
	return 0;
}

/** Unloading function for VFS module.
 * @return		0 on success, negative error code on failure. */
static int vfs_unload(void) {
	return -ERR_NOT_IMPLEMENTED;
}

MODULE_NAME("vfs");
MODULE_DESC("Virtual filesystem (VFS) manager.");
MODULE_FUNCS(vfs_init, vfs_unload);
