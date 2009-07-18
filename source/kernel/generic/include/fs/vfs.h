/* Kiwi virtual filesystem
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
 * @brief		Virtual filesystem (VFS).
 */

#ifndef __FS_VFS_H
#define __FS_VFS_H

#include <mm/aspace.h>

#include <sync/mutex.h>

#include <types/radix.h>
#include <types/refcount.h>

struct vfs_mount;
struct vfs_node;
struct vfs_type;

#if 0
# pragma mark FS node functions/definitions.
#endif

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
	struct vfs_mount *mount;	/**< Mount that the node resides on. */
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

extern int vfs_node_create_from_memory(const char *name, const void *memory, size_t size, vfs_node_t **nodep);

#if 0
# pragma mark Address space functions.
#endif

extern int vfs_aspace_source_create(vfs_node_t *node, int flags, aspace_source_t **sourcep);

#if 0
# pragma mark Mount functions/definitions.
#endif

/** Mount description structure. */
typedef struct vfs_mount {
	list_t header;			/**< Link to mount list. */

	struct vfs_type *type;		/**< Filesystem type. */
	void *data;			/**< Filesystem driver data. */
	int flags;			/**< Flags for the mount. */

	vfs_node_t *root;		/**< Root node for the mount. */
	vfs_node_t *mountpoint;		/**< Directory that this mount is mounted on. */

	mutex_t lock;			/**< Lock to protect node lists. */
	list_t dirty_nodes;		/**< List of unused but dirty nodes. */
	list_t unused_nodes;		/**< List of unused nodes. */
} vfs_mount_t;

/** Mount behaviour flags. */
#define VFS_MOUNT_RDONLY	(1<<0)	/**< Mount is read-only. */

extern vfs_mount_t *vfs_root_mount;

extern int vfs_mount_create(const char *type, int flags, vfs_mount_t **mountp);
extern int vfs_mount_attach(vfs_mount_t *mount, vfs_node_t *node);

#if 0
# pragma mark FS type functions/definitions.
#endif

/** Filesystem type description structure.
 * @note		When adding new required operations to this structure,
 *			add a check to vfs_type_register(). */
typedef struct vfs_type {
	list_t header;			/**< Link to types list. */

	const char *name;		/**< Name of the FS type. */
	refcount_t count;		/**< Reference count of mounts using this FS type. */
	int flags;			/**< Flags specifying traits of this FS type. */

	/**
	 * Main operations.
	 */

	/** Check whether a device contains this filesystem type.
	 * @note		If a filesystem type does not provide this
	 *			function, then it is assumed that the FS does
	 *			not use a backing device (e.g. RamFS).
	 * @param dev		Device to check.
	 * @return		True if the device contains a FS of this type,
	 *			false if not. */
	//bool (*check)(device_t *dev);

	/** Mount a filesystem of this type.
	 * @note		It is guaranteed that the device will contain
	 *			the correct FS type when this is called, as
	 *			the check operation is called prior to this.
	 * @param mount		Mount structure for the mount. This structure
	 *			will contain a pointer to the device the FS
	 *			resides on (will be NULL if no source).
	 * @return		0 on success, negative error code on failure. */
	int (*mount)(vfs_mount_t *mount);

	/** Unmount a filesystem of this type.
	 * @param mount		Mount being unmounted.
	 * @return		0 on success, negative error code on failure. */
	int (*unmount)(vfs_mount_t *mount);

	/**
	 * Page manipulation functions.
	 */

	/** Get a page to use for a node's data.
	 * @note		If this operation is not provided, then the
	 *			VFS will allocate an anonymous, zeroed page via
	 *			pmm_alloc() to use for node data.
	 * @param node		Node to get page for.
	 * @param offset	Offset within the node the page is for.
	 * @param mmflag	Allocation flags.
	 * @param physp		Where to store address of page obtained.
	 * @return		0 on success, negative error code on failure. */
	int (*page_get)(vfs_node_t *node, offset_t offset, int mmflag, phys_ptr_t *physp);

	/** Read a page from a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be read.
	 * @note		If this operation is not provided by a FS
	 *			type, then it is assumed that the page given
	 *			by the page_get operation already contains the
	 *			correct data. The reason this operation is
	 *			provided rather than just having data read
	 *			in by the page_get operation is so that the
	 *			FS implementation does not always have to deal
	 *			with mapping and unmapping physical memory.
	 * @param node		Node being read from.
	 * @param page		Pointer to mapped page to read into.
	 * @param offset	Offset within the file to read from.
	 * @param nonblock	Whether the read is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*page_read)(vfs_node_t *node, void *page, offset_t offset, bool nonblock);

	/** Flush changes to a page within a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be written back. If it is desired
	 *			to resize the file, the node_resize operation
	 *			must be called.
	 * @note		If this operation is not provided, then it
	 *			is assumed that modified pages should always
	 *			remain in the cache until its destruction (for
	 *			example, RamFS does this).
	 * @param node		Node being written to.
	 * @param page		Pointer to mapped page being written.
	 * @param offset	Offset within the file to write to.
	 * @param nonblock	Whether the write is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*page_flush)(vfs_node_t *node, void *page, offset_t offset, bool nonblock);

	/** Free a page previously obtained via get_page.
	 * @note		If this is not provided, then the VFS will
	 *			free the page via pmm_free().
	 * @param node		Node that page was used for.
	 * @param page		Address of page to free. */
	int (*page_free)(vfs_node_t *node, phys_ptr_t page);

	/**
	 * Node modification functions.
	 */

	/** Find a child node.
	 * @param parent	Node to search under.
	 * @param node		Node structure to fill with information about
	 *			child. The name of the child to search for is
	 *			stored in this structure when this function is
	 *			called.
	 * @return		0 on success, negative error code on failure. */
	int (*node_find)(vfs_node_t *parent, vfs_node_t *node);

	/** Clean up data associated with a node.
	 * @param node		Node to clean up. */
	void (*node_free)(vfs_node_t *node);

	/** Create a new filesystem node.
	 * @param parent	Parent directory of the node.
	 * @param node		Node structure describing the node being
	 *			created.
	 * @return		0 on success, negative error code on failure. */
	int (*node_create)(vfs_node_t *parent, vfs_node_t *node);

	/** Modify the size of a node.
	 * @param node		Node being resized.
	 * @param size		New size of the node.
	 * @return		0 on success, negative error code on failure. */
	int (*node_resize)(vfs_node_t *node, file_size_t size);
} vfs_type_t;

/** Filesystem type trait flags. */
#define VFS_TYPE_RDONLY		(1<<0)	/**< Filesystem type is read-only. */

extern int vfs_type_register(vfs_type_t *type);
extern int vfs_type_unregister(vfs_type_t *type);

#endif /* __FS_VFS_H */
