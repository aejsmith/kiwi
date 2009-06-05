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
 * @brief		VFS filesystem mounting functions
 */

#ifndef __FS_MOUNT_H
#define __FS_MOUNT_H

#include <fs/filesystem.h>
#include <fs/node.h>

/** Mount description structure. */
typedef struct vfs_mount {
	vfs_filesystem_t *fs;		/**< Filesystem this mountpoint is for. */
	vfs_node_t *mountpoint;		/**< Directory that this mount is mounted on. */
	struct vfs_mount *parent;	/**< Parent mount. */
} vfs_mount_t;

#endif /* __FS_MOUNT_H */
