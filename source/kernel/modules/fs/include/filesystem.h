/* Kiwi VFS filesystem structure/functions
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
 * @brief		VFS filesystem structure/functions.
 */

#ifndef __FS_FILESYSTEM_H
#define __FS_FILESYSTEM_H

#include <types/refcount.h>

struct device;
struct vfs_node;
struct vfs_type;

/** Structure containing details of a single filesystem. */
typedef struct vfs_filesystem {
	struct device *device;		/**< Device backing this filesystem. */
	struct vfs_type *type;		/**< Type of this filesystem. */
	void *data;			/**< Data used by the filesystem type. */

	struct vfs_node *root;		/**< Root node of the filesystem. */
	refcount_t count;		/**< Number of mounts using the filesystem. */
} vfs_filesystem_t;

#endif /* __FS_FILESYSTEM_H */
