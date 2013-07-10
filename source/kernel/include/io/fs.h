/*
 * Copyright (C) 2009-2013 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Filesystem interface.
 */

#ifndef __IO_FS_H
#define __IO_FS_H

#include <io/file.h>

#include <kernel/fs.h>

#include <lib/avl_tree.h>
#include <lib/radix_tree.h>
#include <lib/refcount.h>

#include <sync/mutex.h>

#include <object.h>

struct fs_mount;
struct fs_node;
struct vm_cache;

/** Structure containing a filesystem mount option. */
typedef struct fs_mount_option {
	const char *name;		/**< Argument name. */
	const char *value;		/**< Argument value (can be NULL). */
} fs_mount_option_t;

/** Filesystem type description structure. */
typedef struct fs_type {
	list_t header;			/**< Link to types list. */

	const char *name;		/**< Short name of the filesystem type. */
	const char *description;	/**< Long name of the type. */
	refcount_t count;		/**< Number of mounts using this type. */

	/** Check whether a device contains this FS type.
	 * @note		If a filesystem type does not provide this
	 *			function, then it is assumed that the FS does
	 *			not use a backing device (e.g. RamFS).
	 * @param device	Handle to device to check.
	 * @param uuid		If not NULL, the function should only return
	 *			true if the filesystem also has this UUID.
	 * @return		Whether the device contains this FS type. */
	bool (*probe)(object_handle_t *device, const char *uuid);

	/** Mount an instance of this FS type.
	 * @note		It is guaranteed that the device will contain
	 *			the correct FS type when this is called, as
	 *			the probe operation is called prior to this.
	 * @note		This function must also create the root node
	 *			and store a pointer to it in the mount
	 *			structure, as though read_node was called
	 *			on it.
	 * @param mount		Mount structure for the mount.
	 * @param opts		Array of options passed to the mount call.
	 * @param count		Number of options in the array.
	 * @return		Status code describing result of the operation. */
	status_t (*mount)(struct fs_mount *mount, fs_mount_option_t *opts, size_t count);
} fs_type_t;

/** Mount operations structure. */
typedef struct fs_mount_ops {
	/** Unmount the filesystem.
	 * @note		Flush is NOT called before this function.
	 * @param mount		Filesystem being unmounted. Filesystem-specific
	 *			data should be freed, but not the structure
	 *			itself. */
	void (*unmount)(struct fs_mount *mount);

	/** Flush changes to filesystem metadata.
	 * @param mount		Mount to flush.
	 * @return		Status code describing result of the operation. */
	status_t (*flush)(struct fs_mount *mount);

	/** Read a node from the filesystem.
	 * @param mount		Mount to obtain node from.
	 * @param id		ID of node to get.
	 * @param nodep		Where to store pointer to node structure.
	 * @return		Status code describing result of the operation. */
	status_t (*read_node)(struct fs_mount *mount, node_id_t id, struct fs_node **nodep);
} fs_mount_ops_t;

/** Node operations structure. */
typedef struct fs_node_ops {
	/** Clean up data associated with a node structure.
	 * @note		This should remove the node from the filesystem
	 *			if the link count is 0.
	 * @param node		Node to clean up. */
	void (*free)(struct fs_node *node);

	/** Flush changes to node metadata.
	 * @param node		Node to flush.
	 * @return		Status code describing result of the operation. */
	status_t (*flush)(struct fs_node *node);

	/** Create a new node as a child of an existing directory.
	 * @param parent	Directory to create in.
	 * @param name		Name to give directory entry.
	 * @param type		Type to give the new node.
	 * @param target	For symbolic links, the target of the link.
	 * @param nodep		Where to store pointer to node structure for
	 *			created entry.
	 * @return		Status code describing result of the operation. */
	status_t (*create)(struct fs_node *parent, const char *name, file_type_t type,
		const char *target, struct fs_node **nodep);

	/** Remove an entry from a directory.
	 * @note		If the node's link count reaches 0, this
	 *			function should call fs_node_remove() on the
	 *			node, but not remove it from the filesystem, as
	 *			it may still be in use. The node will be freed
	 *			as soon as it has no users (it is up to the
	 *			free operation to remove the node from the
	 *			filesystem).
	 * @note		If the node being unlinked is a directory, this
	 *			function should ensure that it is empty (except
	 *			for . and .. entries), and return
	 *			STATUS_DIR_NOT_EMPTY if it isn't.
	 * @param parent	Directory containing the node.
	 * @param name		Name of the node in the directory.
	 * @param node		Node being unlinked.
	 * @return		Status code describing result of the operation. */
	status_t (*unlink)(struct fs_node *parent, const char *name, struct fs_node *node);

	/** Get information about a node.
	 * @param node		Node to get information on.
	 * @param infop		Information structure to fill in. */
	void (*info)(struct fs_node *node, file_info_t *infop);

	/** Modify the size of a file.
	 * @param node		Node being resized.
	 * @param size		New size of the node.
	 * @return		Status code describing result of the operation. */
	status_t (*resize)(struct fs_node *node, offset_t size);

	/** Look up a directory entry.
	 * @param node		Node to look up in.
	 * @param name		Name of entry to look up.
	 * @param idp		Where to store ID of node entry points to.
	 * @return		Status code describing result of the operation. */
	status_t (*lookup)(struct fs_node *node, const char *name, node_id_t *idp);

	/** Read the destination of a symbolic link.
	 * @param node		Node to read from.
	 * @param destp		Where to store pointer to string (must be
	 *			allocated using a kmalloc()-based function)
	 *			containing link destination.
	 * @return		Status code describing result of the operation. */
	status_t (*read_symlink)(struct fs_node *node, char **destp);

	/**
	 * File handle operations.
	 */

	/** Open a handle to the node.
	 * @param node		Node being opened.
	 * @param handle	File handle structure to fill in.
	 * @return		Status code describing result of the operation. */
	status_t (*open)(struct fs_node *node, struct file_handle *handle);

	/** Close a handle to the node.
	 * @param node		Node being closed.
	 * @param handle	File handle structure. */
	void (*close)(struct fs_node *node, struct file_handle *handle);

	/** Perform I/O on a file.
	 * @param node		Node to perform I/O on.
	 * @param handle	File handle structure.
	 * @param request	I/O request.
	 * @param bytesp	Where to store total number of bytes transferred.
	 * @return		Status code describing result of the operation. */
	status_t (*io)(struct fs_node *node, struct file_handle *handle,
		struct io_request *request, size_t *bytesp);

	/** Get the data cache for a file.
	 * @param node		Node to get cache for.
	 * @param handle	Handle that the cache is needed for.
	 * @return		Pointer to node's VM cache. If this function is
	 *			provided, it is assumed that this function will
	 *			always succeed, otherwise it is assumed that
	 *			the file cannot be memory-mapped. */
	struct vm_cache *(*get_cache)(struct fs_node *node, struct file_handle *handle);

	/** Read the next directory entry.
	 * @note		It is up to the filesystem implementation to
	 *			store the current offset into the directory.
	 *			It can make use of the offset field in the
	 *			handle to do so. This field is set to 0 when
	 *			the handle is opened and when rewind_dir() is
	 *			called on it, otherwise it is not modified.
	 * @param node		Node to read from.
	 * @param handle	File handle structure.
	 * @param entryp	Where to store pointer to directory entry
	 *			structure (must be allocated using a
	 *			kmalloc()-based function).
	 * @return		Status code describing result of the operation. */
	status_t (*read_dir)(struct fs_node *node, struct file_handle *handle,
		dir_entry_t **entryp);
} fs_node_ops_t;

/** Structure containing details of a mounted filesystem. */
typedef struct fs_mount {
	mutex_t lock;			/**< Lock to protect structure. */

	avl_tree_t nodes;		/**< Tree mapping node IDs to node structures. */
	list_t used_nodes;		/**< List of in-use nodes. */
	list_t unused_nodes;		/**< List of unused nodes (LRU first). */

	int flags;			/**< Flags for the mount. */
	fs_mount_ops_t *ops;		/**< Mount operations. */
	void *data;			/**< Implementation data pointer. */
	object_handle_t *device;	/**< Handle to device that the filesystem resides on. */

	struct fs_node *root;		/**< Root node for the mount. */
	struct fs_node *mountpoint;	/**< Directory that this mount is mounted on. */

	mount_id_t id;			/**< Mount ID. */
	fs_type_t *type;		/**< Filesystem type. */
	list_t header;			/**< Link to mounts list. */
} fs_mount_t;

/** Structure containing details of a filesystem node. */
typedef struct fs_node {
	file_t file;			/**< File object header. */

	refcount_t count;		/**< Number of references to the node. */
	avl_tree_node_t tree_link;	/**< Link to node tree. */
	list_t mount_link;		/**< Link to mount's node lists. */
	list_t unused_link;		/**< Link to global unused node list. */
	node_id_t id;			/**< ID of the node. */
	unsigned flags;			/**< Flags for the node. */
	fs_mount_t *mounted;		/**< Pointer to filesystem mounted on this node. */

	fs_node_ops_t *ops;		/**< Node operations. */
	void *data;			/**< Internal data pointer for filesystem type. */
	fs_mount_t *mount;		/**< Mount that the node resides on. */
} fs_node_t;

/** Flags for a filesystem node. */
#define FS_NODE_REMOVED		(1<<0)	/**< Node has been removed. */

/** Macro to check if a node is read-only. */
#define FS_NODE_IS_READ_ONLY(node) \
	((node)->mount && (node)->mount->flags & FS_MOUNT_READ_ONLY)

/**
 * Functions for use by filesystem implementations.
 */

extern status_t fs_type_register(fs_type_t *type);
extern status_t fs_type_unregister(fs_type_t *type);

extern fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, file_type_t type,
	fs_node_ops_t *ops, void *data);
extern void fs_node_retain(fs_node_t *node);
extern void fs_node_release(fs_node_t *node);
extern void fs_node_remove(fs_node_t *node);

/**
 * Kernel interface.
 */

extern status_t fs_open(const char *path, object_rights_t rights, uint32_t flags,
	unsigned create, object_handle_t **handlep);

extern status_t fs_create_dir(const char *path);
extern status_t fs_create_fifo(const char *path);
extern status_t fs_create_symlink(const char *path, const char *target);

extern status_t fs_read_symlink(const char *path, char *buf, size_t size);

extern status_t fs_mount(const char *device, const char *path, const char *type,
	uint32_t flags, const char *opts);
extern status_t fs_unmount(const char *path);

extern status_t fs_info(const char *path, bool follow, file_info_t *info);
extern status_t fs_link(const char *source, const char *dest);
extern status_t fs_unlink(const char *path);
extern status_t fs_rename(const char *source, const char *dest);
extern status_t fs_sync(void);

/**
 * Initialization/shutdown functions.
 */

extern void fs_init(void);
extern void fs_shutdown(void);

#endif /* __IO_FS_H */
