/* Kiwi VFS node structure/functions
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
 * @brief		VFS node structure/functions.
 */

#ifndef __FS_NODE_H
#define __FS_NODE_H

#include <fs/filesystem.h>

#include <sync/mutex.h>

#include <types/radix.h>
#include <types/refcount.h>

/** Type used to store a file size. */
typedef uint64_t file_size_t;

/** Filesystem node type definitions. */
typedef enum vfs_node_type {
        VFS_NODE_REGULAR,               /**< Regular file. */
        VFS_NODE_DIR,                   /**< Directory. */
        VFS_NODE_BLKDEV,                /**< Block device. */
        VFS_NODE_CHRDEV,                /**< Character device. */
        VFS_NODE_FIFO,                  /**< FIFO (named pipe). */
        VFS_NODE_SYMLINK,               /**< Symbolic link. */
        VFS_NODE_SOCK,                  /**< Socket. */
} vfs_node_type_t;

/** Structure describing a single node in a filesystem. */
typedef struct vfs_node {
	/** Basic node information. */
	char *name;			/**< Name of the node. */
	vfs_node_type_t type;		/**< Type of the node. */
	vfs_filesystem_t *fs;		/**< Filesystem the node resides on. */

	/** Synchronization variables. */
	mutex_t lock;			/**< Lock to protect the node. */
	refcount_t count;		/**< Reference count to track users of the node. */

	/** Node tree information. */
	struct vfs_node *parent;	/**< Parent node (NULL if node is root of FS). */
	radix_tree_t children;		/**< Tree of child nodes. */
	size_t child_count;		/**< Number of child nodes. */
} vfs_node_t;

#endif /* __FS_NODE_H */
