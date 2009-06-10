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

#ifndef __FS_MOUNT_H
#define __FS_MOUNT_H

#include <sync/mutex.h>

#include <types/list.h>

struct vfs_node;
struct vfs_type;

/** Mount description structure. */
typedef struct vfs_mount {
	list_t header;			/**< Link to mount list. */

	struct vfs_type *type;		/**< Filesystem type. */
	void *data;			/**< Filesystem driver data. */
	int flags;			/**< Flags for the mount. */

	struct vfs_node *root;		/**< Root node for the mount. */
	struct vfs_node *mountpoint;	/**< Directory that this mount is mounted on. */

	mutex_t lock;			/**< Lock to protect node lists. */
	list_t dirty_nodes;		/**< List of unused but dirty nodes. */
	list_t unused_nodes;		/**< List of unused nodes. */
} vfs_mount_t;

/** Mount behaviour flags. */
#define VFS_MOUNT_RDONLY	(1<<0)	/**< Mount is read-only. */

extern vfs_mount_t *vfs_root_mount;

extern int vfs_mount_create(const char *type, int flags, vfs_mount_t **mountp);
extern int vfs_mount_attach(vfs_mount_t *mount, struct vfs_node *node);

#endif /* __FS_MOUNT_H */
