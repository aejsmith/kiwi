/*
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
 * @brief		Virtual file system (VFS).
 */

#ifndef __IO_VFS_H
#define __IO_VFS_H

#include <lib/avl.h>
#include <lib/radix.h>
#include <lib/refcount.h>

#include <mm/vm.h>

#include <sync/mutex.h>

#include <limits.h>

struct device;
struct kernel_args;
struct vfs_info;
struct vfs_mount;
struct vfs_node;

/** Filesystem type description structure.
 * @note		When adding new required operations to this structure,
 *			add a check to fs_type_register(). */
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
	 * @param device	Device to check.
	 * @return		True if the device contains a FS of this type,
	 *			false if not. */
	bool (*probe)(struct device *device);

	/** Mount an instance of this filesystem type.
	 * @note		It is guaranteed that the device will contain
	 *			the correct FS type when this is called, as
	 *			the probe operation is called prior to this.
	 * @note		This function should fill in details for the
	 *			root filesystem node as though node_get were
	 *			called on it.
	 * @param mount		Mount structure for the FS. This structure will
	 *			contain a pointer to the device the FS resides
	 *			on (if the filesystem has no source, the device
	 *			pointer will be NULL).
	 * @return		0 on success, negative error code on failure. */
	int (*mount)(struct vfs_mount *mount);

	/** Unmount an instance of this filesystem.
	 * @param mount		Mount that's being unmounted. */
	void (*unmount)(struct vfs_mount *mount);

	/**
	 * Data modification functions.
	 */

	/** Read a page of data from a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be read.
	 * @note		If not provided, then pages will be filled with
	 *			zeros.
	 * @param node		Node to read data read from.
	 * @param page		Pointer to mapped page to read into.
	 * @param offset	Offset within the file to read from (multiple
	 *			of PAGE_SIZE).
	 * @param nonblock	Whether the read is required to not block.
	 * @return		0 on success, negative error code on failure. */
	int (*page_read)(struct vfs_node *node, void *page, offset_t offset, bool nonblock);

	/** Flush changes to a page within a node.
	 * @note		If the page straddles across the end of the
	 *			file, then only the part of the file that
	 *			exists should be written back. If it is desired
	 *			to resize the file, the file_resize operation
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
	int (*page_flush)(struct vfs_node *node, const void *page, offset_t offset, bool nonblock);

	/**
	 * Node manipulation functions.
	 */

	/** Fill out a node structure with details of a node.
	 * @param node		Node structure to fill out.
	 * @param id		ID of node that structure is for.
	 * @return		0 on success, negative error code on failure. */
	int (*node_get)(struct vfs_node *node, identifier_t id);

	/** Flush changes to node metadata.
	 * @param node		Node to flush.
	 * @return		0 on success, negative error code on failure. */
	int (*node_flush)(struct vfs_node *node);

	/** Clean up data associated with a node structure.
	 * @note		This should remove the node from the filesystem
	 *			if the link count is 0.
	 * @param node		Node to clean up. */
	void (*node_free)(struct vfs_node *node);

	/** Create a new filesystem node.
	 * @note		It is up to this function to create the
	 *			directory entry for the node on the real
	 *			filesystem. The VFS will handle adding the
	 *			entry to the directory entry cache.
	 * @note		When this function returns success, details in 
	 *			the node structure should be filled in
	 *			(including a node ID) as though node_get had
	 *			also been called on it.
	 * @param parent	Parent directory of the node.
	 * @param name		Name to give node in the parent directory.
	 * @param node		Node structure describing the node being
	 *			created. For symbolic links, the link_dest
	 *			pointer in the node will point to a string
	 *			containing the link destination.
	 * @return		0 on success, negative error code on failure. */
	int (*node_create)(struct vfs_node *parent, const char *name, struct vfs_node *node);

	/** Decrease the link count of a filesystem node.
	 * @note		If the count reaches 0, this should set the
	 *			VFS_NODE_REMOVED flag on the node, but not
	 *			remove it from the filesystem, as it may still
	 *			be in use. This flag will cause the node to be
	 *			freed immediately when the reference count
	 *			reaches 0 - it is up to the node_free operation
	 *			to remove the node from the FS if necessary.
	 *			Also note that if the VFS_NODE_REMOVED flag is
	 *			set, then the node's metadata and cached data
	 *			will NOT be flushed when the node is freed.
	 * @param parent	Directory containing the node.
	 * @param name		Name of the node in the directory.
	 * @param node		Node being unlinked.
	 * @return		0 on success, negative error code on failure. */
	int (*node_unlink)(struct vfs_node *parent, const char *name, struct vfs_node *node);

	/** Get information about a node.
	 * @param node		Node to get information on.
	 * @param info		Information structure to fill in. */
	void (*node_info)(struct vfs_node *node, struct vfs_info *info);

	/**
	 * Regular file functions.
	 */

	/** Modify the size of a file.
	 * @param node		Node being resized.
	 * @param size		New size of the node.
	 * @return		0 on success, negative error code on failure. */
	int (*file_resize)(struct vfs_node *node, file_size_t size);

	/** Open a file.
	 * @param node		Node being opened (will be VFS_NODE_FILE).
	 * @param flags		Flags to fs_file_open().
	 * @return		0 on success, negative error code on failure. */
	int (*file_open)(struct vfs_node *node, int flags);

	/** Close a file.
	 * @param node		Node being closed. */
	void (*file_close)(struct vfs_node *node);

	/**
	 * Directory functions.
	 */

	/** Cache directory contents.
	 * @note		In order to add a directory entry to the cache,
	 *			the vfs_dir_entry_add() function should be
	 *			used.
	 * @param node		Node to cache contents of.
	 * @return		0 on success, negative error code on failure. */
	int (*dir_cache)(struct vfs_node *node);

	/** Open a directory.
	 * @param node		Directory being opened.
	 * @param flags		Flags to fs_dir_open().
	 * @return		0 on success, negative error code on failure. */
	int (*dir_open)(struct vfs_node *node, int flags);

	/** Close a directory.
	 * @param node		Node being closed. */
	void (*dir_close)(struct vfs_node *node);

	/**
	 * Symbolic link functions.
	 */

	/** Get the destination of a symbolic link.
	 * @param node		Symbolic link to get destination of.
	 * @param bufp		Where to store pointer to string containing
	 *			link destination. This buffer must have been
	 *			allocated using kmalloc() (or kstrdup(), as it
	 *			is implemented using kmalloc()).
	 * @return		0 on success, negative error code on failure. */
	int (*symlink_read)(struct vfs_node *node, char **bufp);
} vfs_type_t;

/** Structure describing a mounted filesystem. */
typedef struct vfs_mount {
	list_t header;			/**< Link to mounts list. */

	mutex_t lock;			/**< Lock to protect structure. */
	identifier_t id;		/**< Mount ID. */
	vfs_type_t *type;		/**< Filesystem type. */
	void *data;			/**< Filesystem type data. */
	struct device *device;		/**< Device that the filesystem resides on. */
	int flags;			/**< Flags for the mount. */

	struct vfs_node *root;		/**< Root node for the mount. */
	struct vfs_node *mountpoint;	/**< Directory that this mount is mounted on. */

	avl_tree_t nodes;		/**< Tree mapping node IDs to node structures. */
	list_t used_nodes;		/**< List of in-use nodes. */
	list_t unused_nodes;		/**< List of unused nodes (in LRU order). */
} vfs_mount_t;

/** Structure describing a node in the filesystem. */
typedef struct vfs_node {
	vm_object_t vobj;		/**< VM object header. */
	list_t header;			/**< Link to mount's node lists. */

	mutex_t lock;			/**< Lock to protect the node. */
	refcount_t count;		/**< Reference count to track users of the node. */
	identifier_t id;		/**< Identifier of the node. */
	vfs_mount_t *mount;		/**< Mount that the node resides on. */
	void *data;			/**< Internal data pointer for filesystem type. */
	int flags;			/**< Behaviour flags for the node. */
	vfs_mount_t *mounted;		/**< Pointer to filesystem mounted on this node. */

	/** Type of the node. */
	enum {
		VFS_NODE_FILE,		/**< Regular file. */
		VFS_NODE_DIR,		/**< Directory. */
		VFS_NODE_SYMLINK,	/**< Symbolic link. */
		VFS_NODE_BLKDEV,	/**< Block device. */
		VFS_NODE_CHRDEV,	/**< Character device. */
		VFS_NODE_FIFO,		/**< FIFO (named pipe). */
		VFS_NODE_SOCK,		/**< Socket. */
	} type;

	avl_tree_t pages;		/**< Tree of cached data pages (VFS_NODE_FILE). */
	radix_tree_t dir_entries;	/**< Tree of cached directory entries (VFS_NODE_DIR). */
	file_size_t size;		/**< Total size of node data/number of cached directory entries. */
	char *link_dest;		/**< Cached symlink destination (VFS_NODE_SYMLINK). */
} vfs_node_t;

/** Data for a VFS handle (both handle types need the same data). */
typedef struct vfs_handle {
	mutex_t lock;			/**< Lock to protect offset. */
	vfs_node_t *node;		/**< Node that the handle refers to. */
	offset_t offset;		/**< Current file offset. */
	int flags;			/**< Flags the file was opened with. */
} vfs_handle_t;

/** Directory entry information structure. */
typedef struct vfs_dir_entry {
	size_t length;			/**< Length of this structure including name. */
	identifier_t id;		/**< ID of the node for the entry. */
	char name[];			/**< Name of entry. */
} vfs_dir_entry_t;

/** Filesystem node information structure. */
typedef struct vfs_info {
	identifier_t id;		/**< Node ID. */
	identifier_t mount;		/**< Mount ID. */
	size_t blksize;			/**< I/O block size. */
	file_size_t size;		/**< Total size of node data on filesystem. */
	size_t links;			/**< Number of links to the node. */
} vfs_info_t;

/** Filesystem type trait flags. */
#define VFS_TYPE_RDONLY		(1<<0)	/**< Filesystem type is read-only. */
#define VFS_TYPE_CACHE_BASED	(1<<1)	/**< Filesystem type is cache-based - all nodes will remain in memory. */

/** Mount behaviour flags. */
#define VFS_MOUNT_RDONLY	(1<<0)	/**< Mount is read-only. */

/** Node behaviour flags. */
#define VFS_NODE_REMOVED	(1<<0)	/**< Node should be freed immediately when its reference count reaches 0. */

/** Macro to check if a node is read-only. */
#define VFS_NODE_IS_RDONLY(node)	((node)->mount && (node)->mount->flags & VFS_MOUNT_RDONLY)

extern vfs_mount_t *vfs_root_mount;

extern int vfs_type_register(vfs_type_t *type);
extern int vfs_type_unregister(vfs_type_t *type);

extern int vfs_node_lookup(const char *path, bool follow, int type, vfs_node_t **nodep);
extern void vfs_node_get(vfs_node_t *node);
extern void vfs_node_release(vfs_node_t *node);
extern void vfs_node_info(vfs_node_t *node, vfs_info_t *info);

extern int vfs_file_create(const char *path, vfs_node_t **nodep);
extern int vfs_file_from_memory(const void *buf, size_t size, vfs_node_t **nodep);
extern int vfs_file_read(vfs_node_t *node, void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int vfs_file_write(vfs_node_t *node, const void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int vfs_file_resize(vfs_node_t *node, file_size_t size);

extern void vfs_dir_entry_add(vfs_node_t *node, identifier_t id, const char *name);
extern int vfs_dir_create(const char *path, vfs_node_t **nodep);
extern int vfs_dir_read(vfs_node_t *node, vfs_dir_entry_t *buf, size_t size, offset_t index);

extern int vfs_symlink_create(const char *path, const char *target, vfs_node_t **nodep);
extern int vfs_symlink_read(vfs_node_t *node, char *buf, size_t size);

extern int vfs_mount(const char *dev, const char *path, const char *type, int flags);
extern int vfs_unmount(const char *path);
extern int vfs_unlink(const char *path);

extern void vfs_mount_root(struct kernel_args *args);
extern void vfs_init(void);

extern int kdbg_cmd_mounts(int argc, char **argv);
extern int kdbg_cmd_vnodes(int argc, char **argv);
extern int kdbg_cmd_vnode(int argc, char **argv);

/** Behaviour flags for fs_file_open(). */
#define FS_FILE_READ		0x0001	/**< Open for reading. */
#define FS_FILE_WRITE		0x0002	/**< Open for writing. */
#define FS_FILE_APPEND		0x0004	/**< Before each write, offset is set to the end of the file. */
#define FS_FILE_NONBLOCK	0x0008	/**< Read/write operations on the file will not block. */

/** Behaviour flags for fs_dir_open(). */
#define FS_DIR_NONBLOCK		0x0001	/**< Read operations on the directory should not block. */

/** Operations for fs_handle_seek(). */
#define FS_HANDLE_SEEK_SET	1	/**< Set the offset to the exact position specified. */
#define FS_HANDLE_SEEK_ADD	2	/**< Add the supplied value to the current offset. */
#define FS_HANDLE_SEEK_END	3	/**< Set the offset to the end of the file plus the supplied value. */

extern int sys_fs_file_create(const char *path);
extern handle_t sys_fs_file_open(const char *path, int flags);
extern int sys_fs_file_read(handle_t handle, void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int sys_fs_file_write(handle_t handle, const void *buf, size_t count, offset_t offset, size_t *bytesp);
extern int sys_fs_file_resize(handle_t handle, file_size_t size);

extern int sys_fs_dir_create(const char *path);
extern handle_t sys_fs_dir_open(const char *path, int flags);
extern int sys_fs_dir_read(handle_t handle, vfs_dir_entry_t *buf, size_t size, offset_t index);

extern int sys_fs_handle_seek(handle_t handle, int action, offset_t offset, offset_t *newp);
extern int sys_fs_handle_info(handle_t handle, vfs_info_t *infop);
extern int sys_fs_handle_sync(handle_t handle);

extern int sys_fs_symlink_create(const char *path, const char *target);
extern int sys_fs_symlink_read(const char *path, char *buf, size_t size);

extern int sys_fs_mount(const char *dev, const char *path, const char *type, int flags);
extern int sys_fs_unmount(const char *path);
extern int sys_fs_getcwd(char *buf, size_t size);
extern int sys_fs_setcwd(const char *path);
extern int sys_fs_setroot(const char *path);
extern int sys_fs_info(const char *path, bool follow, vfs_info_t *infop);
extern int sys_fs_link(const char *source, const char *dest);
extern int sys_fs_unlink(const char *path);
extern int sys_fs_rename(const char *source, const char *dest);

#endif /* __IO_VFS_H */
