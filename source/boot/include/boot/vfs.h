/*
 * Copyright (C) 2010 Alex Smith
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
 * @brief		Filesystem classes.
 */

#ifndef __BOOT_VFS_H
#define __BOOT_VFS_H

#include <lib/list.h>
#include <lib/refcount.h>
#include <lib/utility.h>

struct disk;
struct vfs_filesystem;
struct vfs_node;

/** Structure containing operations for a disk device. */
typedef struct disk_ops_t {
	/** Read a block from the disk.
	 * @param disk		Disk being read from.
	 * @param buf		Buffer to read into.
	 * @param lba		Block number to read.
	 * @return		Whether reading succeeded. */
	bool (*block_read)(struct disk *disk, void *buf, offset_t lba);
} disk_ops_t;

/** Structure representing a disk device. */
typedef struct disk {
	uint8_t id;			/**< ID of the disk. */
	size_t blksize;			/**< Size of one block on the disk. */
	file_size_t blocks;		/**< Number of blocks on the disk. */
	disk_ops_t *ops;		/**< Pointer to operations structure. */
	void *data;			/**< Implementation-specific data pointer. */
	char *partial_block;		/**< Block for partial transfers. */
} disk_t;

/** Structure containing operations for a filesystem. */
typedef struct vfs_filesystem_ops {
	/** Create an instance of this filesystem.
	 * @param fs		Filesystem structure to fill in.
	 * @return		Whether succeeded in mounting. */
	bool (*mount)(struct vfs_filesystem *fs);

	/** Read a node from the filesystem.
	 * @param fs		Filesystem to read from.
	 * @param id		ID of node.
	 * @return		Pointer to node on success, NULL on failure. */
	struct vfs_node *(*node_get)(struct vfs_filesystem *fs, inode_t id);

	/** Read from a file.
	 * @param node		Node referring to file.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset into the file.
	 * @return		Whether read successfully. */
	bool (*file_read)(struct vfs_node *node, void *buf, size_t count, offset_t offset);

	/** Cache directory entries.
	 * @param node		Node to cache entries from.
	 * @return		Whether cached successfully. */
	bool (*dir_cache)(struct vfs_node *node);
} vfs_filesystem_ops_t;

/** Structure representing a mounted filesystem. */
typedef struct vfs_filesystem {
	list_t header;			/**< Link to filesystems list. */

	vfs_filesystem_ops_t *ops;	/**< Operations for the filesystem. */
	void *data;			/**< Implementation-specific data pointer. */
	disk_t *disk;			/**< Disk that the filesystem resides on. */
	char *label;			/**< Label of the filesystem. */
	char *uuid;			/**< UUID of the filesystem. */
	struct vfs_node *root;		/**< Root of the filesystem. */
	list_t nodes;			/**< List of nodes. */
} vfs_filesystem_t;

/** Structure representing a filesystem node. */
typedef struct vfs_node {
	list_t header;			/**< Link to filesystem's node list. */

	vfs_filesystem_t *fs;		/**< Filesystem that the node is on. */
	inode_t id;			/**< Node number. */

	/** Type of the node. */
	enum {
		VFS_NODE_FILE,		/**< Regular file. */
		VFS_NODE_DIR,		/**< Directory. */
	} type;

	refcount_t count;		/**< Reference count. */
	file_size_t size;		/**< Size of the file. */
	void *data;			/**< Implementation-specific data pointer. */

	list_t entries;			/**< Directory entries. */
} vfs_node_t;

/** Structure representing a directory entry. */
typedef struct vfs_dir_entry {
	list_t header;			/**< Link to entry list. */

	char *name;			/**< Name of entry. */
	inode_t id;			/**< Node ID entry refers to. */
} vfs_dir_entry_t;

extern list_t filesystem_list;
extern vfs_filesystem_t *boot_filesystem;
extern char *boot_path_override;

extern vfs_node_t *vfs_filesystem_lookup(vfs_filesystem_t *fs, const char *path);
extern vfs_node_t *vfs_filesystem_boot_path(vfs_filesystem_t *fs);

extern vfs_node_t *vfs_node_alloc(vfs_filesystem_t *fs, inode_t id, int type, file_size_t size, void *data);
extern void vfs_node_acquire(vfs_node_t *node);
extern void vfs_node_release(vfs_node_t *node);

extern bool vfs_file_read(vfs_node_t *node, void *buf, size_t count, offset_t offset);

extern void vfs_dir_insert(vfs_node_t *node, char *name, inode_t id);
extern vfs_node_t *vfs_dir_lookup(vfs_node_t *node, const char *path);
extern vfs_dir_entry_t *vfs_dir_iterate(vfs_node_t *node, vfs_dir_entry_t *prev);

extern bool disk_read(disk_t *disk, void *buf, size_t count, offset_t offset);
extern disk_t *disk_add(uint8_t id, size_t blksize, file_size_t blocks, disk_ops_t *ops,
                        void *data, bool boot);

extern void platform_disk_detect(void);
extern void disk_init(void);

#endif /* __BOOT_VFS_H */
