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

#include <lib/avl.h>
#include <lib/radix.h>
#include <lib/refcount.h>

#include <mm/cache.h>

#include <public/fs.h>

#include <sync/mutex.h>

#include <limits.h>
#include <object.h>

struct fs_mount;
struct fs_node;

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
	 * @return		0 on success, negative error code on failure. */
	int (*mount)(struct fs_mount *mount, fs_mount_option_t *opts, size_t count);
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
	 * @return		0 on success, negative error code on failure. */
	int (*flush)(struct fs_mount *mount);

	/** Read a node from the filesystem.
	 * @param mount		Mount to obtain node from.
	 * @param id		ID of node to get.
	 * @param nodep		Where to store pointer to node structure.
	 * @return		0 on success, negative error code on failure. */
	int (*read_node)(struct fs_mount *mount, node_id_t id, struct fs_node **nodep);
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
	 * @return		0 on success, negative error code on failure. */
	int (*flush)(struct fs_node *node);

	/** Read from a file.
	 * @note		This function is provided to allow special
	 *			handling for read operations to be done. If
	 *			not provided, the file cache will be used.
	 *			This function does not have to read the data,
	 *			as depending on the return code the file cache
	 *			may be used to read the data.
	 * @param node		Node to read from.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset into file to read from.
	 * @param nonblock	Whether the write is required to not block.
	 * @param bytesp	Where to store number of bytes read.
	 * @return		If 1 is returned, then the function should have
	 *			handled the entire operation itself and the
	 *			file cache will not be used. If 0 is returned,
	 *			the file cache will be used to perform the
	 *			read. If a negative value is returned, then it
	 *			is taken as an error. */
	int (*read)(struct fs_node *node, void *buf, size_t count, offset_t offset,
	            bool nonblock, size_t *bytesp);

	/** Write to a file.
	 * @note		This function is provided to allow special
	 *			handling for write operations to be done (such
	 *			as ensuring enough space is reserved on the
	 *			filesystem). If not provided, the file cache
	 *			will be used. This function does not have to
	 *			write the data, as depending on the return code
	 *			the file cache may be used to write the data.
	 * @param node		Node to write to.
	 * @param buf		Buffer containing data to write.
	 * @param count		Number of bytes to write.
	 * @param offset	Offset into file to write to.
	 * @param nonblock	Whether the write is required to not block.
	 * @param bytesp	Where to store number of bytes written.
	 * @return		If 1 is returned, then the function should have
	 *			handled the entire operation itself and the
	 *			file cache will not be used. If 0 is returned,
	 *			the file cache will be used to perform the
	 *			write. If a negative value is returned, then it
	 *			is taken as an error. */
	int (*write)(struct fs_node *node, const void *buf, size_t count, offset_t offset,
	             bool nonblock, size_t *bytesp);

	/** Read a page of data from a file.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be read.
	 * @note		If not provided, then pages will be filled with
	 *			zeros.
	 * @param node		Node to read from.
	 * @param buf		Buffer to read into.
	 * @param offset	Offset within the file to read from (multiple
	 *			of PAGE_SIZE).
	 * @param nonblock	Whether the read is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*read_page)(struct fs_node *node, void *buf, offset_t offset, bool nonblock);

	/** Write a page of data to a file.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be written back.
	 * @note		If this operation is not provided, then it
	 *			is assumed that pages should always remain in
	 *			the cache until its destruction (for example,
	 *			RamFS does this).
	 * @param node		Node to write to.
	 * @param buf		Buffer containing data to write.
	 * @param offset	Offset within the file to write to (multiple
	 *			of PAGE_SIZE).
	 * @param nonblock	Whether the write is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*write_page)(struct fs_node *node, const void *buf, offset_t offset, bool nonblock);

	/** Modify the size of a file.
	 * @param node		Node being resized.
	 * @param size		New size of the node.
	 * @return		0 on success, negative error code on failure. */
	int (*resize)(struct fs_node *node, offset_t size);

	/** Create a new node as a child of an existing directory.
	 * @note		This function only has to create the directory
	 *			entry on the filesystem - the entry will be
	 *			cached when the function returns.
	 * @note		When this function returns success, details in 
	 *			the node structure should be filled in
	 *			(including a node ID) as though get_node had
	 *			also been called on it.
	 * @param parent	Directory to create in.
	 * @param name		Name to give directory entry.
	 * @param node		Node structure describing the node being
	 *			created. For symbolic links, the link_dest
	 *			pointer in the node will point to a string
	 *			containing the link destination.
	 * @return		0 on success, negative error code on failure. */
	int (*create)(struct fs_node *parent, const char *name, struct fs_node *node);

	/** Decrease the link count of a filesystem node.
	 * @note		If the count reaches 0, this function should
	 *			call fs_node_remove() on the node, but not
	 *			remove it from the filesystem, as it may still
	 *			be in use. The node will be freed as soon as
	 *			it has no users (it is up to the free operation
	 *			to remove the node from the filesystem).
	 * @param parent	Directory containing the node.
	 * @param name		Name of the node in the directory.
	 * @param node		Node being unlinked.
	 * @return		0 on success, negative error code on failure. */
	int (*unlink)(struct fs_node *parent, const char *name, struct fs_node *node);

	/** Get information about a node.
	 * @param node		Node to get information on.
	 * @param info		Information structure to fill in. */
	void (*info)(struct fs_node *node, fs_info_t *info);

	/** Cache directory contents.
	 * @note		In order to add a directory entry to the cache,
	 *			the fs_dir_insert() function should be
	 *			used.
	 * @param node		Node to cache contents of.
	 * @return		0 on success, negative error code on failure. */
	int (*cache_children)(struct fs_node *node);

	/** Store the destination of a symbolic link.
	 * @note		This function should set the link_cache pointer
	 *			in the node structure to a pointer to a string,
	 *			allocated using kmalloc() or a kmalloc()-based
	 *			function, which contains the link destination.
	 * @param node		Symbolic link to cache destination of.
	 * @return		0 on success, negative error code on failure. */
	int (*cache_dest)(struct fs_node *node);
} fs_node_ops_t;

/** Structure containing details of a mounted filesystem. */
typedef struct fs_mount {
	mutex_t lock;			/**< Lock to protect structure. */

	avl_tree_t nodes;		/**< Tree mapping node IDs to node structures. */
	list_t used_nodes;		/**< List of in-use nodes. */
	list_t unused_nodes;		/**< List of unused nodes (in LRU order). */

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

/** Structure containing a directory entry cache. */
typedef struct fs_dir_cache {
	radix_tree_t entries;		/**< Tree of name to entry mappings. */
	size_t count;			/**< Number of entries. */
} fs_dir_cache_t;

/** Structure containing details of a filesystem node. */
typedef struct fs_node {
	object_t obj;                   /**< Object header. */

	mutex_t lock;			/**< Lock to protect the node. */
	refcount_t count;		/**< Number of references to the node. */
	list_t mount_link;		/**< Link to mount's node lists. */
	node_id_t id;			/**< ID of the node. */
	fs_node_type_t type;		/**< Type of the node. */
	int flags;			/**< Behaviour flags for the node. */
	fs_mount_t *mounted;		/**< Pointer to filesystem mounted on this node. */

	fs_node_ops_t *ops;		/**< Node operations. */
	void *data;			/**< Internal data pointer for filesystem type. */
	fs_mount_t *mount;		/**< Mount that the node resides on. */

	/** Pointers to cached data. */
	union {
		/** Data cache (FS_NODE_FILE). */
		vm_cache_t *data_cache;

		/** Directory entry cache (FS_NODE_DIR). */
		fs_dir_cache_t *entry_cache;

		/** Symbolic link destination (FS_NODE_SYMLINK). */
		char *link_cache;
	};
} fs_node_t;

/** Node behaviour flags. */
#define FS_NODE_REMOVED		(1<<0)	/**< Node has been removed from the filesystem. */

/** Macro to check if a node is read-only. */
#define FS_NODE_IS_RDONLY(node)	\
	((node)->mount && (node)->mount->flags & FS_MOUNT_RDONLY)

/**
 * Functions for use by filesystem implementations.
 */

extern int fs_type_register(fs_type_t *type);
extern int fs_type_unregister(fs_type_t *type);

extern fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, fs_node_type_t type,
                                fs_node_ops_t *ops, void *data);
extern void fs_node_release(fs_node_t *node);
extern void fs_node_remove(fs_node_t *node);

extern void fs_dir_insert(fs_node_t *node, const char *name, node_id_t id);

/**
 * Kernel interface.
 */

extern int fs_file_create(const char *path);
extern int fs_file_from_memory(const void *buf, size_t size, int flags, object_handle_t **handlep);
extern int fs_file_open(const char *path, int flags, object_handle_t **handlep);
extern int fs_file_read(object_handle_t *handle, void *buf, size_t count, size_t *bytesp);
extern int fs_file_pread(object_handle_t *handle, void *buf, size_t count, offset_t offset,
                         size_t *bytesp);
extern int fs_file_write(object_handle_t *handle, const void *buf, size_t count, size_t *bytesp);
extern int fs_file_pwrite(object_handle_t *handle, const void *buf, size_t count,
                          offset_t offset, size_t *bytesp);
extern int fs_file_resize(object_handle_t *handle, offset_t size);

extern int fs_dir_create(const char *path);
extern int fs_dir_open(const char *path, int flags, object_handle_t **handlep);
extern int fs_dir_read(object_handle_t *handle, fs_dir_entry_t *buf, size_t size);

extern int fs_handle_seek(object_handle_t *handle, int action, rel_offset_t offset, offset_t *newp);
extern int fs_handle_info(object_handle_t *handle, fs_info_t *info);
extern int fs_handle_sync(object_handle_t *handle);

extern int fs_symlink_create(const char *path, const char *target);
extern int fs_symlink_read(const char *path, char *buf, size_t size);

extern int fs_mount(const char *dev, const char *path, const char *type, const char *opts);
extern int fs_unmount(const char *path);
//extern int fs_sync(void);
extern int fs_info(const char *path, bool follow, fs_info_t *info);
//extern int fs_link(const char *source, const char *dest);
extern int fs_unlink(const char *path);
//extern int fs_rename(const char *source, const char *dest);

/**
 * Debugger commands.
 */

extern int kdbg_cmd_mount(int argc, char **argv);
extern int kdbg_cmd_node(int argc, char **argv);

/**
 * Initialisation functions.
 */

extern void fs_mount_root(struct kernel_args *args);
extern void fs_init(void);

#endif /* __IO_FS_H */
