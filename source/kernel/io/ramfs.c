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

#include <io/vfs.h>

#include <mm/malloc.h>

#include <sync/mutex.h>

#include <errors.h>
#include <init.h>

/** RamFS mount information structure. */
typedef struct ramfs_mount {
	mutex_t lock;			/**< Lock to protect ID allocation. */
	identifier_t next_id;		/**< Next node ID. */
} ramfs_mount_t;

extern vfs_type_t ramfs_fs_type;

/** Mount a RamFS.
 * @param mount		Mount structure for the FS.
 * @return		0 on success, negative error code on failure. */
static int ramfs_mount(vfs_mount_t *mount) {
	ramfs_mount_t *data;

	mount->data = data = kmalloc(sizeof(ramfs_mount_t), MM_SLEEP);
	mutex_init(&data->lock, "ramfs_mount_lock", 0);

	/* The root node has its ID as 0, and set the next ID to 1. */
	mount->root->id = 0;
	data->next_id = 1;

	/* Add a '.' entry and a fake '..' entry to the root node. */
	vfs_dir_entry_add(mount->root, mount->root->id, ".");
	vfs_dir_entry_add(mount->root, mount->root->id, "..");
	return 0;
}

/** Unmount a RamFS.
 * @param mount		Mount that's being unmounted. */
static void ramfs_unmount(vfs_mount_t *mount) {
	kfree(mount->data);
}

/** Create a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param node		Node structure describing the node being created.
 * @return		0 on success, negative error code on failure. */
static int ramfs_node_create(vfs_node_t *parent, const char *name, vfs_node_t *node) {
	ramfs_mount_t *mount = parent->mount->data;

	/* Allocate a unique ID for the node. I would just test whether
	 * (next ID + 1) < next ID, but according to GCC, signed overflow
	 * doesn't happen...
	 * http://archives.postgresql.org/pgsql-hackers/2005-12/msg00635.php */
	mutex_lock(&mount->lock, 0);
	if(mount->next_id == INT32_MAX) {
		mutex_unlock(&mount->lock);
		return -ERR_NO_SPACE;
	}
	node->id = mount->next_id++;
	mutex_unlock(&mount->lock);

	/* If we're creating a directory, add '.' and '..' entries to it. Other
	 * directoriy entries will be maintained by the VFS. */
	if(node->type == VFS_NODE_DIR) {
		vfs_dir_entry_add(node, node->id, ".");
		vfs_dir_entry_add(node, parent->id, "..");
	}

	return 0;
}

/** Unlink a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param node		Node structure describing the node being created.
 * @return		0 on success, negative error code on failure. */
static int ramfs_node_unlink(vfs_node_t *parent, const char *name, vfs_node_t *node) {
	/* The VFS handles removing the node from the parent's entry cache. */
	node->flags |= VFS_NODE_REMOVED;
	return 0;
}

/** Resize a RamFS file.
 * @param node		Node to resize.
 * @param size		New size of the node.
 * @return		Always returns 0. */
static int ramfs_file_resize(vfs_node_t *node, file_size_t size) {
	return 0;
}

/** RamFS filesystem type structure */
vfs_type_t ramfs_fs_type = {
	.name = "ramfs",
	.flags = VFS_TYPE_CACHE_BASED,

	.mount = ramfs_mount,
	.unmount = ramfs_unmount,
	.node_create = ramfs_node_create,
	.node_unlink = ramfs_node_unlink,
	.file_resize = ramfs_file_resize,
};

/** Register RamFS with the VFS. */
static void __init_text ramfs_init(void) {
	int ret;

	if((ret = vfs_type_register(&ramfs_fs_type)) != 0) {
		fatal("Could not register RamFS filesystem type (%d)", ret);
	}
}
INITCALL(ramfs_init);