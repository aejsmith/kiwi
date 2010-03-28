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
 * @brief		RAM-based temporary filesystem.
 */

#include <io/fs.h>

#include <mm/malloc.h>

#include <errors.h>
#include <init.h>

/** RamFS mount information structure. */
typedef struct ramfs_mount {
	mutex_t lock;			/**< Lock to protect ID allocation. */
	node_id_t next_id;		/**< Next node ID. */
} ramfs_mount_t;

/** RamFS node information structure. */
typedef struct ramfs_node {
	offset_t size;			/**< Size of the file. */
} ramfs_node_t;

/** Free a RamFS node.
 * @param node		Node to free. */
static void ramfs_node_free(fs_node_t *node) {
	kfree(node->data);
}

/** Resize a RamFS file.
 * @param node		Node to resize.
 * @param size		New size of the node.
 * @return		Always returns 0. */
static int ramfs_node_resize(fs_node_t *node, offset_t size) {
	ramfs_node_t *data = node->data;
	data->size = size;
	return 0;
}

/** Create a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param node		Node structure describing the node being created.
 * @return		0 on success, negative error code on failure. */
static int ramfs_node_create(fs_node_t *parent, const char *name, fs_node_t *node) {
	ramfs_mount_t *mount = parent->mount->data;
	ramfs_node_t *data;

	/* Allocate a unique ID for the node. I would just test whether
	 * (next ID + 1) < next ID, but according to GCC, signed overflow
	 * doesn't happen...
	 * http://archives.postgresql.org/pgsql-hackers/2005-12/msg00635.php */
	mutex_lock(&mount->lock);
	if(mount->next_id == INT32_MAX) {
		mutex_unlock(&mount->lock);
		return -ERR_NO_SPACE;
	}
	node->id = mount->next_id++;
	mutex_unlock(&mount->lock);

	/* Create the information structure. */
	data = node->data = kmalloc(sizeof(ramfs_node_t), MM_SLEEP);
	data->size = 0;

	/* If we're creating a directory, add '.' and '..' entries to it. Other
	 * directory entries will be maintained by the FS layer. */
	if(node->type == FS_NODE_DIR) {
		fs_dir_insert(node, ".", node->id);
		fs_dir_insert(node, "..", parent->id);
	}
	return 0;
}

/** Unlink a RamFS filesystem node.
 * @param parent	Parent directory of the node.
 * @param name		Name to give the node.
 * @param node		Node structure describing the node being created.
 * @return		0 on success, negative error code on failure. */
static int ramfs_node_unlink(fs_node_t *parent, const char *name, fs_node_t *node) {
	fs_node_remove(node);
	return 0;
}

/** Get information about a RamFS node.
 * @param node		Node to get information on.
 * @param info		Information structure to fill in. */
static void ramfs_node_info(fs_node_t *node, fs_info_t *info) {
	ramfs_node_t *data = node->data;
	info->size = data->size;
}

/** RamFS node operations structure. */
static fs_node_ops_t ramfs_node_ops = {
	.free = ramfs_node_free,
	.resize = ramfs_node_resize,
	.create = ramfs_node_create,
	.unlink = ramfs_node_unlink,
	.info = ramfs_node_info,
};

/** Unmount a RamFS.
 * @param mount		Mount that's being unmounted. */
static void ramfs_unmount(fs_mount_t *mount) {
	kfree(mount->data);
}

/** RamFS mount operations structure. */
static fs_mount_ops_t ramfs_mount_ops = {
	.unmount = ramfs_unmount,
};

/** Mount a RamFS filesystem.
 * @param mount		Mount structure for the FS.
 * @param opts		Array of mount options.
 * @param count		Number of options.
 * @return		0 on success, negative error code on failure. */
static int ramfs_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
	ramfs_mount_t *data;

	mount->ops = &ramfs_mount_ops,
	mount->data = data = kmalloc(sizeof(ramfs_mount_t), MM_SLEEP);
	mutex_init(&data->lock, "ramfs_mount_lock", 0);
	data->next_id = 1;

	/* Create the root directory, and add '.' and '..' entries. */
	mount->root = fs_node_alloc(mount, 0, FS_NODE_DIR, &ramfs_node_ops, NULL);
	mount->root->data = kcalloc(1, sizeof(ramfs_node_t), MM_SLEEP);
	fs_dir_insert(mount->root, ".", mount->root->id);
	fs_dir_insert(mount->root, "..", mount->root->id);
	return 0;
}

/** RamFS filesystem type structure. */
static fs_type_t ramfs_fs_type = {
	.name = "ramfs",
	.description = "RAM-based temporary filesystem",
	.mount = ramfs_mount,
};

/** Register RamFS with the VFS. */
static void __init_text ramfs_init(void) {
	int ret;

	if((ret = fs_type_register(&ramfs_fs_type)) != 0) {
		fatal("Could not register RamFS filesystem type (%d)", ret);
	}
}
INITCALL(ramfs_init);
