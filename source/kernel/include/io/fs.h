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
 * @brief		Filesystem interface.
 */

#ifndef __IO_FS_H
#define __IO_FS_H

#include <kernel/fs.h>

#include <lib/avl_tree.h>
#include <lib/radix_tree.h>
#include <lib/refcount.h>

#include <sync/mutex.h>

#include <object.h>

struct device;
struct fs_mount;
struct fs_node;
struct kernel_args;
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
	 * @param security	Security attributes for the node. This should
	 *			be passed through to fs_node_alloc() when
	 *			creating the node structure.
	 * @param nodep		Where to store pointer to node structure for
	 *			created entry.
	 * @return		Status code describing result of the operation. */
	status_t (*create)(struct fs_node *parent, const char *name, file_type_t type,
	                   const char *target, object_security_t *security,
	                   struct fs_node **nodep);

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

	/** Update security attributes of a node.
	 * @param node		Node to set for.
	 * @param security	New security attributes to set. If the user
	 *			or group ID are -1, or the ACL pointer is NULL,
	 *			they should not be changed.
	 * @return		Status code describing result of the operation. */
	status_t (*set_security)(struct fs_node *node, const object_security_t *security);

	/** Read from a file.
	 * @param node		Node to read from.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset into file to read from.
	 * @param nonblock	Whether the write is required to not block.
	 * @param bytesp	Where to store number of bytes read.
	 * @return		Status code describing result of the operation. */
	status_t (*read)(struct fs_node *node, void *buf, size_t count, offset_t offset,
	                 bool nonblock, size_t *bytesp);

	/** Write to a file.
	 * @note		It is up to this function to resize the file
	 *			if necessary, resize will not be called before
	 *			this function.
	 * @param node		Node to write to.
	 * @param buf		Buffer containing data to write.
	 * @param count		Number of bytes to write.
	 * @param offset	Offset into file to write to.
	 * @param nonblock	Whether the write is required to not block.
	 * @param bytesp	Where to store number of bytes written.
	 * @return		Status code describing result of the operation. */
	status_t (*write)(struct fs_node *node, const void *buf, size_t count, offset_t offset,
	                  bool nonblock, size_t *bytesp);

	/** Get the data cache for a file.
	 * @param node		Node to get cache for.
	 * @return		Pointer to node's VM cache. If this function is
	 *			provided, it is assumed that this function will
	 *			always succeed, otherwise it is assumed that
	 *			the file cannot be memory-mapped. */
	struct vm_cache *(*get_cache)(struct fs_node *node);

	/** Modify the size of a file.
	 * @param node		Node being resized.
	 * @param size		New size of the node.
	 * @return		Status code describing result of the operation. */
	status_t (*resize)(struct fs_node *node, offset_t size);

	/** Read a directory entry.
	 * @param node		Node to read from.
	 * @param index		Index of entry to read.
	 * @param entryp	Where to store pointer to directory entry
	 *			structure (must be allocated using a
	 *			kmalloc()-based function).
	 * @return		Status code describing result of the operation. */
	status_t (*read_entry)(struct fs_node *node, offset_t index, dir_entry_t **entryp);

	/** Look up a directory entry.
	 * @param node		Node to look up in.
	 * @param name		Name of entry to look up.
	 * @param idp		Where to store ID of node entry points to.
	 * @return		Status code describing result of the operation. */
	status_t (*lookup_entry)(struct fs_node *node, const char *name, node_id_t *idp);

	/** Read the destination of a symbolic link.
	 * @param node		Node to read from.
	 * @param destp		Where to store pointer to string (must be
	 *			allocated using a kmalloc()-based function)
	 *			containing link destination.
	 * @return		Status code describing result of the operation. */
	status_t (*read_link)(struct fs_node *node, char **destp);
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

/** Mount behaviour flags. */
#define FS_MOUNT_RDONLY		(1<<0)	/**< Mount is read-only. */

/** Structure containing details of a filesystem node. */
typedef struct fs_node {
	object_t obj;                   /**< Object header. */

	refcount_t count;		/**< Number of references to the node. */
	avl_tree_node_t tree_link;	/**< Link to node tree. */
	list_t mount_link;		/**< Link to mount's node lists. */
	list_t unused_link;		/**< Link to global unused node list. */
	node_id_t id;			/**< ID of the node. */
	file_type_t type;		/**< Type of the node. */
	bool removed;			/**< Whether the node has been removed. */
	fs_mount_t *mounted;		/**< Pointer to filesystem mounted on this node. */

	fs_node_ops_t *ops;		/**< Node operations. */
	void *data;			/**< Internal data pointer for filesystem type. */
	fs_mount_t *mount;		/**< Mount that the node resides on. */
} fs_node_t;

/** Macro to check if a node is read-only. */
#define FS_NODE_IS_RDONLY(node)	\
	((node)->mount && (node)->mount->flags & FS_MOUNT_RDONLY)

/**
 * Functions for use by filesystem implementations.
 */

extern status_t fs_type_register(fs_type_t *type);
extern status_t fs_type_unregister(fs_type_t *type);

extern fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, file_type_t type,
                                object_security_t *security, fs_node_ops_t *ops,
                                void *data);
extern void fs_node_release(fs_node_t *node);
extern void fs_node_remove(fs_node_t *node);


/**
 * Kernel interface.
 */

extern object_handle_t *file_from_memory(const void *buf, size_t size);
extern status_t file_open(const char *path, object_rights_t rights, int flags,
                          int create, object_security_t *security,
                          object_handle_t **handlep);
extern status_t file_read(object_handle_t *handle, void *buf, size_t count, size_t *bytesp);
extern status_t file_pread(object_handle_t *handle, void *buf, size_t count, offset_t offset,
                           size_t *bytesp);
extern status_t file_write(object_handle_t *handle, const void *buf, size_t count,
                           size_t *bytesp);
extern status_t file_pwrite(object_handle_t *handle, const void *buf, size_t count,
                            offset_t offset, size_t *bytesp);
extern status_t file_resize(object_handle_t *handle, offset_t size);
extern status_t file_seek(object_handle_t *handle, int action, rel_offset_t offset,
                          offset_t *newp);
extern status_t file_info(object_handle_t *handle, file_info_t *infop);
extern status_t file_sync(object_handle_t *handle);

extern status_t dir_create(const char *path, object_security_t *security);
extern status_t dir_read(object_handle_t *handle, dir_entry_t *buf, size_t size);

extern status_t symlink_create(const char *path, const char *target);
extern status_t symlink_read(const char *path, char *buf, size_t size);

extern void fs_probe(struct device *device);
extern status_t fs_mount(const char *device, const char *path, const char *type, const char *opts);
extern status_t fs_unmount(const char *path);
extern status_t fs_info(const char *path, bool follow, file_info_t *infop);
//extern status_t fs_link(const char *source, const char *dest);
extern status_t fs_unlink(const char *path);
//extern status_t fs_rename(const char *source, const char *dest);
//extern status_t fs_sync(void);

/**
 * Debugger commands.
 */

extern int kdbg_cmd_mount(int argc, char **argv);
extern int kdbg_cmd_node(int argc, char **argv);

/**
 * Initialisation/shutdown functions.
 */

extern void fs_init(struct kernel_args *args);
extern void fs_shutdown(void);

#endif /* __IO_FS_H */
