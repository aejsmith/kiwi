/* Kiwi VFS filesystem mounting functions
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
 * @brief		VFS filesystem mounting functions.
 */

#include <mm/malloc.h>

#include "vfs_priv.h"

/** Pointer to mount at root of the filesystem. */
vfs_mount_t *vfs_root_mount = NULL;

/** List of all mounts. */
static LIST_DECLARE(vfs_mount_list);
static MUTEX_DECLARE(vfs_mount_list_lock, 0);

/** Free all nodes on the given list.
 * @param list		List to reclaim from.
 * @return		Whether anything was reclaimed. */
static bool vfs_mount_reclaim_nodes_internal(list_t *list) {
	bool freed = false;
	vfs_node_t *node;

	/* Keep on looping through the lists until we can do nothing else. */
	LIST_FOREACH_SAFE(list, iter) {
		node = list_entry(iter, vfs_node_t, header);

		if(vfs_node_free(node, false) == 0) {
			freed = true;
		}
	}

	return freed;
}

/** Reclaim unused nodes from all mounts. */
void vfs_mount_reclaim_nodes(void) {
	vfs_mount_t *mount;

	mutex_lock(&vfs_mount_list_lock, 0);

	/* For each mount, keep on looping over it, freeing each node that we
	 * come to, until nothing else can be done. */
	LIST_FOREACH(&vfs_mount_list, iter) {
		mount = list_entry(iter, vfs_mount_t, header);

		mutex_lock(&mount->lock, 0);
		while(vfs_mount_reclaim_nodes_internal(&mount->unused_nodes) ||
		      vfs_mount_reclaim_nodes_internal(&mount->dirty_nodes));
		mutex_unlock(&mount->lock);
	}

	mutex_unlock(&vfs_mount_list_lock);
}

/** Create a new mount.
 *
 * Mounts a filesystem and creates a a mount structure for it.
 *
 * @param type		Name of filesystem type.
 * @param flags		Behaviour flags for the mount.
 * @param mountp	Where to store pointer to mount structure.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_mount_create(const char *type, int flags, vfs_mount_t **mountp) {
	vfs_mount_t *mount;
	int ret;

	if(mountp == NULL) {
		return -ERR_PARAM_INVAL;
	}

	/* Create a mount structure for the mount. */
	mount = kmalloc(sizeof(vfs_mount_t), MM_SLEEP);
	mount->flags = NULL;
	mount->mountpoint = NULL;

	list_init(&mount->header);
	list_init(&mount->dirty_nodes);
	list_init(&mount->unused_nodes);
	mutex_init(&mount->lock, "vfs_mount_lock", 0);

	/* Look up the filesystem type. */
	mount->type = vfs_type_lookup(type, true);
	if(mount->type == NULL) {
		kfree(mount);
		return -ERR_PARAM_INVAL;
	}

	/* If the type is read-only, set read-only in the mount flags. */
	if(mount->type->flags & VFS_TYPE_RDONLY) {
		mount->flags |= VFS_MOUNT_RDONLY;
	}

	/* Create the root node for the filesystem. */
	mount->root = vfs_node_alloc(NULL, mount, MM_SLEEP);

	/* Call the filesystem's mount operation. */
	if(mount->type->mount) {
		ret = mount->type->mount(mount);
		if(ret != 0) {
			vfs_node_release(mount->root);
			vfs_node_free(mount->root, true);
			refcount_dec(&mount->type->count);
			kfree(mount);
			return ret;
		}
	}

	/* Store mount in mounts list. */
	mutex_lock(&vfs_mount_list_lock, 0);
	list_append(&vfs_mount_list, &mount->header);
	mutex_unlock(&vfs_mount_list_lock);

	dprintf("vfs: mounted filesystem %p(%s) (mount: %p, root: %p)\n",
	        mount->type, mount->type->name, mount, mount->root);
	*mountp = mount;
	return 0;
}
MODULE_EXPORT(vfs_mount_create);

/** Attach a mount to a filesystem node.
 *
 * Attaches a mount created with vfs_mount_create() to an existing node within
 * the filesystem.
 *
 * @param mount		Mount to attach.
 * @param node		Node to attach mount to. Must be a directory.
 *
 * @return		0 on success, negative error code on failure.
 */
int vfs_mount_attach(vfs_mount_t *mount, vfs_node_t *node) {
	return -ERR_NOT_IMPLEMENTED;
}
MODULE_EXPORT(vfs_mount_attach);
