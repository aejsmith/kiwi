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

#include <fs/mount.h>

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
	list_t header;			/**< Link to node lists. */

	/** Basic node information. */
	char *name;			/**< Name of the node. */
	vfs_node_type_t type;		/**< Type of the node. */
	vfs_mount_t *mount;		/**< Mount that the node resides on. */
	int flags;			/**< Behaviour flags for the node. */

	/** Node data information. */
	struct cache *cache;		/**< Cache containing node data. */
	file_size_t size;		/**< Total size of node data. */
	bool dirty;			/**< Whether any part of the node's data is dirty. */

	/** Synchronization information. */
	mutex_t lock;			/**< Lock to protect the node. */
	refcount_t count;		/**< Reference count to track users of the node. */

	/** Node tree information. */
	struct vfs_node *parent;	/**< Parent node (NULL if node is root of FS). */
	radix_tree_t children;		/**< Tree of child nodes. */
} vfs_node_t;

/** Filesystem node behaviour flags. */
#define VFS_NODE_PERSISTENT	(1<<0)	/**< Node should stay in memory until the FS is destroyed. */

extern int vfs_node_lookup(vfs_node_t *from, const char *path, vfs_node_t **nodep);
extern void vfs_node_get(vfs_node_t *node);
extern void vfs_node_release(vfs_node_t *node);

extern int vfs_node_create(vfs_node_t *parent, const char *name, vfs_node_type_t type, vfs_node_t **nodep);
extern int vfs_node_read(vfs_node_t *node, void *buffer, size_t count, offset_t offset, size_t *bytesp);
extern int vfs_node_write(vfs_node_t *node, const void *buffer, size_t count, offset_t offset, size_t *bytesp);

extern int vfs_node_create_from_memory(const void *memory, size_t size, vfs_node_t **nodep);

#endif /* __FS_NODE_H */
