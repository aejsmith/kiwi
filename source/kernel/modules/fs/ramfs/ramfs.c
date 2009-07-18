/* Kiwi RAM-based temporary filesystem
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
 * @brief		RAM-based temporary filesystem.
 */

#include <fs/vfs.h>

#include <errors.h>
#include <module.h>

/** Create a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param node		Node structure describing the node being created.
 * @return		0 on success, negative error code on failure. */
static int ramfs_node_create(vfs_node_t *parent, vfs_node_t *node) {
	/* RamFS is entirely cache based, make sure the node remains in the
	 * node cache. */
	node->flags |= VFS_NODE_PERSISTENT;
	return 0;
}

/** Resize a RamFS filesystem node.
 * @param node		Node to resize.
 * @param size		New size of the node.
 * @return		Always returns 0. */
static int ramfs_node_resize(vfs_node_t *node, file_size_t size) {
	return 0;
}

/** RamFS filesystem type structure */
static vfs_type_t ramfs_fs_type = {
	.name = "ramfs",
	.node_create = ramfs_node_create,
	.node_resize = ramfs_node_resize,
};

/** Initialization function for RamFS.
 * @return		0 on success, negative error code on failure. */
static int ramfs_init(void) {
	return vfs_type_register(&ramfs_fs_type);
}

/** Unloading function for RamFS module.
 * @return		0 on success, negative error code on failure. */
static int ramfs_unload(void) {
	return vfs_type_unregister(&ramfs_fs_type);
}

MODULE_NAME("ramfs");
MODULE_DESC("RAM-based temporary filesystem driver.");
MODULE_FUNCS(ramfs_init, ramfs_unload);
