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
 * @brief		Filesystem functions.
 */

#ifndef __BOOT_FS_H
#define __BOOT_FS_H

#include <boot/disk.h>

#include <lib/list.h>
#include <lib/refcount.h>
#include <lib/utility.h>

struct fs_mount;
struct fs_node;

/** Structure containing operations for a filesystem. */
typedef struct fs_type {
	/** Mount an instance of this filesystem.
	 * @param mount		Mount structure to fill in.
	 * @return		Whether succeeded in mounting. */
	bool (*mount)(struct fs_mount *mount);

	/** Read a node from the filesystem.
	 * @param mount		Mount to read from.
	 * @param id		ID of node.
	 * @return		Pointer to node on success, NULL on failure. */
	struct fs_node *(*read_node)(struct fs_mount *mount, node_id_t id);

	/** Read from a file.
	 * @param node		Node referring to file.
	 * @param buf		Buffer to read into.
	 * @param count		Number of bytes to read.
	 * @param offset	Offset into the file.
	 * @return		Whether read successfully. */
	bool (*read_file)(struct fs_node *node, void *buf, size_t count, offset_t offset);

	/** Cache directory entries.
	 * @param node		Node to cache entries from.
	 * @return		Whether cached successfully. */
	bool (*read_dir)(struct fs_node *node);
} fs_type_t;

/** Structure representing a mounted filesystem. */
typedef struct fs_mount {
	list_t header;			/**< Link to mounts list. */

	fs_type_t *type;		/**< Type structure for the filesystem. */
	void *data;			/**< Implementation-specific data pointer. */
	disk_t *disk;			/**< Disk that the filesystem resides on. */
	char *label;			/**< Label of the filesystem. */
	char *uuid;			/**< UUID of the filesystem. */
	struct fs_node *root;		/**< Root of the filesystem. */
	list_t nodes;			/**< List of nodes. */
} fs_mount_t;

/** Structure representing a filesystem node. */
typedef struct fs_node {
	list_t header;			/**< Link to filesystem's node list. */

	fs_mount_t *mount;		/**< Mount that the node is on. */
	node_id_t id;			/**< Node number. */

	/** Type of the node. */
	enum {
		FS_NODE_FILE,		/**< Regular file. */
		FS_NODE_DIR,		/**< Directory. */
	} type;

	refcount_t count;		/**< Reference count. */
	offset_t size;			/**< Size of the file. */
	void *data;			/**< Implementation-specific data pointer. */

	list_t entries;			/**< Directory entries. */
} fs_node_t;

/** Structure representing a directory entry. */
typedef struct fs_dir_entry {
	list_t header;			/**< Link to entry list. */
	char *name;			/**< Name of entry. */
	node_id_t id;			/**< Node ID entry refers to. */
} fs_dir_entry_t;

extern list_t filesystem_list;
extern fs_mount_t *boot_filesystem;
extern char *boot_path_override;

extern fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, int type, offset_t size, void *data);
extern void fs_node_get(fs_node_t *node);
extern void fs_node_release(fs_node_t *node);

extern fs_node_t *fs_lookup(fs_mount_t *mount, const char *path);
extern fs_node_t *fs_find_boot_path(fs_mount_t *mount);

extern bool fs_file_read(fs_node_t *node, void *buf, size_t count, offset_t offset);

extern void fs_dir_insert(fs_node_t *node, char *name, node_id_t id);
extern fs_node_t *fs_dir_lookup(fs_node_t *node, const char *name);
extern fs_dir_entry_t *fs_dir_iterate(fs_node_t *node, fs_dir_entry_t *prev);

#endif /* __BOOT_FS_H */
