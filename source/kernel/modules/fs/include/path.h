/* Kiwi VFS path lookup functions
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
 * @brief		VFS path lookup functions.
 */

#ifndef __FS_PATH_H
#define __FS_PATH_H

#include <fs/mount.h>
#include <fs/namespace.h>
#include <fs/node.h>

/** Path lookup structure. */
typedef struct vfs_path {
	vfs_node_t *node;		/**< Node the path corresponds to. */
	vfs_mount_t *mount;		/**< Mount that the node was found on. */
} vfs_path_t;

extern int vfs_path_get(vfs_path_t *path);
extern int vfs_path_release(vfs_path_t *path);

extern int vfs_path_lookup(vfs_path_t *path, const char *str);

#endif /* __FS_PATH_H */
