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
 * @brief		Virtual file system.
 */

#include <boot/memory.h>
#include <boot/vfs.h>

#include <lib/string.h>

#include <assert.h>

#include "fs/ext2.h"
#include "fs/iso9660.h"

#include "partitions/msdos.h"

/** List of all detected filesystems. */
LIST_DECLARE(filesystem_list);

/** The filesystem being booted from. */
vfs_filesystem_t *boot_filesystem = NULL;

/** Boot path override string. */
char *boot_path_override = NULL;

/** Array of paths to search for boot files. */
static const char *boot_paths[] = {
	"/system/boot",
	"/kiwi",
};

/** Array of filesystem implementations. */
static vfs_filesystem_ops_t *filesystem_types[] = {
	&ext2_filesystem_ops,
	&iso9660_filesystem_ops,
};

/** Array of partition probe functions. */
static bool (*partition_probe_funcs[])(disk_t *) = {
	msdos_partition_probe,
};

/** Probe a disk for filesystems.
 * @param disk		Disk to probe.
 * @return		Pointer to FS if detected, NULL if not. */
static vfs_filesystem_t *vfs_filesystem_probe(disk_t *disk) {
	vfs_filesystem_t *fs;
	vfs_node_t *node;
	size_t i;

	fs = kmalloc(sizeof(vfs_filesystem_t));
	list_init(&fs->header);
	list_init(&fs->nodes);
	fs->disk = disk;

	for(i = 0; i < ARRAYSZ(filesystem_types); i++) {
		fs->ops = filesystem_types[i];
		if(!fs->ops->mount(fs)) {
			continue;
		}

		/* Check if bootable. */
		if(!(node = vfs_filesystem_boot_path(fs))) {
			break;
		}
		vfs_node_release(node);

		list_append(&filesystem_list, &fs->header);
		return fs;
	}

	kfree(fs);
	return NULL;
}

/** Look up a node by ID on a filesystem.
 * @param fs		Filesystem to get from.
 * @param id		ID of node to get.
 * @return		Pointer to node or NULL if not found. */
static vfs_node_t *vfs_filesystem_get_node(vfs_filesystem_t *fs, inode_t id) {
	vfs_node_t *node;

	/* Search in the node cache first. */
	LIST_FOREACH(&fs->nodes, iter) {
		node = list_entry(iter, vfs_node_t, header);
		if(node->id == id) {
			vfs_node_acquire(node);
			return node;
		}
	}

	/* Try to get the node from the filesystem. */
	if(!(node = fs->ops->node_get(fs, id))) {
		return NULL;
	}

	/* Cache the retreived node. */
	list_append(&fs->nodes, &node->header);
	return node;
}

/** Look up a path on a filesystem.
 * @param fs		Filesystem to look up on.
 * @param path		Path to look up.
 * @return		Pointer to node on success, NULL on failure. */
vfs_node_t *vfs_filesystem_lookup(vfs_filesystem_t *fs, const char *path) {
	assert(path[0] == '/');
	while(path[0] == '/') {
		path++;
	}
	return vfs_dir_lookup(fs->root, path);
}

/** Get the node referring to the boot directory.
 * @param fs		Filesystem to get from.
 * @return		Node for boot directory, or NULL if not bootable. */
vfs_node_t *vfs_filesystem_boot_path(vfs_filesystem_t *fs) {
	vfs_node_t *node;
	size_t i;

	if(boot_path_override) {
		if((node = vfs_filesystem_lookup(fs, boot_path_override))) {
			if(node->type == VFS_NODE_DIR) {
				return node;
			}
			vfs_node_release(node);
		}
		return NULL;
	}

	/* Check whether each of the boot paths exists and is a directory. */
	for(i = 0; i < ARRAYSZ(boot_paths); i++) {
		if((node = vfs_filesystem_lookup(fs, boot_paths[i]))) {
			if(node->type == VFS_NODE_DIR) {
				return node;
			}
			vfs_node_release(node);
		}
	}

	return NULL;
}

/** Allocate a new node structure.
 * @param fs		Filesystem that the node resides on.
 * @param id		ID of the node.
 * @param type		Type of the node.
 * @param size		Size of the file data (if a file).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to node structure. */
vfs_node_t *vfs_node_alloc(vfs_filesystem_t *fs, inode_t id, int type, file_size_t size, void *data) {
	vfs_node_t *node = kmalloc(sizeof(vfs_node_t));

	list_init(&node->header);
	refcount_set(&node->count, 1);
	list_init(&node->entries);
	node->fs = fs;
	node->id = id;
	node->type = type;
	node->size = size;
	node->data = data;
	return node;
}

/** Increase the reference count of a node.
 * @param node		Node to increase count of. */
void vfs_node_acquire(vfs_node_t *node) {
	node->count++;
}

/** Decrease the reference count of a node.
 * @param node		Node to decrease count of. */
void vfs_node_release(vfs_node_t *node) {
	assert(node->count > 0);
	if(--node->count == 0) {
		/* FIXME: Don't actually do anything right now, as kfree() is
		 * not implemented - if we free things, we'll actually end up
		 * running out of heap quicker. */
	}
}

/** Read from a file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the file to read from.
 * @return		Whether the read was successful. */
bool vfs_file_read(vfs_node_t *node, void *buf, size_t count, offset_t offset) {
	assert(node->type == VFS_NODE_FILE);
	return node->fs->ops->file_read(node, buf, count, offset);
}

/** Get a child from a directory.
 * @param node		Node to get from.
 * @param name		Name of entry to get.
 * @param idp		Where to store ID of entry.
 * @return		Whether entry was found. */
static bool vfs_dir_get_child(vfs_node_t *node, const char *name, inode_t *idp) {
	vfs_dir_entry_t *entry;

	if(list_empty(&node->entries)) {
		if(!node->fs->ops->dir_cache(node)) {
			return false;
		}
	}

	LIST_FOREACH(&node->entries, iter) {
		entry = list_entry(iter, vfs_dir_entry_t, header);
		if(strcmp(entry->name, name) == 0) {
			*idp = entry->id;
			return true;
		}
	}

	return false;
}

/** Insert an entry into a directory.
 * @param node		Node to add to.
 * @param name		Name of the entry to add (will be duplicated).
 * @param id		ID of the node the entry refers to. */
void vfs_dir_insert(vfs_node_t *node, char *name, inode_t id) {
	vfs_dir_entry_t *entry;

	assert(node->type == VFS_NODE_DIR);

	entry = kmalloc(sizeof(vfs_dir_entry_t));
	list_init(&entry->header);
	entry->name = kstrdup(name);
	entry->id = id;

	list_append(&node->entries, &entry->header);
}

/** Look up a path relative to a directory.
 * @param node		Node to look up from.
 * @param path		Path to look up.
 * @return		Pointer to node on success, NULL on failure. */
vfs_node_t *vfs_dir_lookup(vfs_node_t *node, const char *path) {
	vfs_node_t *child;
	char *dup, *tok;
	inode_t id;

	vfs_node_acquire(node);

	/* Loop through each element of the path string. The string must be
	 * duplicated so that it can be modified. */
	dup = kstrdup(path);
	while(true) {
		tok = strsep(&dup, "/");
		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			return node;
		} else if(node->type != VFS_NODE_DIR) {
			/* The previous node was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			vfs_node_release(node);
			return NULL;
		} else if(!tok[0]) {
			/* Zero-length path component, do nothing. */
			continue;
		}

		/* Look up the token in the directory cache. */
		if(!vfs_dir_get_child(node, tok, &id)) {
			vfs_node_release(node);
			return NULL;
		}

		/* Get the node from the filesystem. */
		if(!(child = vfs_filesystem_get_node(node->fs, id))) {
			vfs_node_release(node);
			return NULL;
		}

		vfs_node_release(node);
		node = child;
	}
}

/** Iterate through entries in a directory.
 * @param node		Node for directory.
 * @param prev		Previous entry (or NULL to start from beginning).
 * @return		Pointer to entry structure for next entry. */
vfs_dir_entry_t *vfs_dir_iterate(vfs_node_t *node, vfs_dir_entry_t *prev) {
	assert(node->type == VFS_NODE_DIR);

	if(list_empty(&node->entries)) {
		if(!node->fs->ops->dir_cache(node)) {
			return NULL;
		}
		if(list_empty(&node->entries)) {
			return NULL;
		}
	}

	if(prev) {
		if(&prev->header == node->entries.prev) {
			return NULL;
		}
		return list_entry(prev->header.next, vfs_dir_entry_t, header);
	} else {
		return list_entry(node->entries.next, vfs_dir_entry_t, header);
	}
}

/** Read from a disk.
 * @param disk		Disk to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the disk to read from.
 * @return		Whether the read was successful. */
bool disk_read(disk_t *disk, void *buf, size_t count, offset_t offset) {
	size_t blksize = disk->blksize;
	uint64_t start, end, i, size;

	/* Allocate a temporary buffer for partial transfers if required. */
	if((offset % blksize || count % blksize) && !disk->partial_block) {
		disk->partial_block = kmalloc(blksize);
	}

	/* Now work out the start block and the end block. Subtract one from
	 * count to prevent end from going onto the next block when the offset
	 * plus the count is an exact multiple of the block size. */
	start = offset / blksize;
	end = (offset + (count - 1)) / blksize;

	/* If we're not starting on a block boundary, we need to do a partial
	 * transfer on the initial block to get up to a block boundary. 
	 * If the transfer only goes across one block, this will handle it. */
	if(offset % blksize) {
		/* Read the block into the temporary buffer. */
		if(!disk->ops->block_read(disk, disk->partial_block, start)) {
			return false;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, disk->partial_block + (offset % blksize), size);
		buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	for(i = 0; i < size; i++, buf += blksize, count -= blksize, start++) {
		/* Read directly into the destination buffer. */
		if(!disk->ops->block_read(disk, buf, start)) {
			return false;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!disk->ops->block_read(disk, disk->partial_block, start)) {
			return false;
		}

		memcpy(buf, disk->partial_block, count);
	}

	return true;
}

/** Read a block from a partition.
 * @param disk		Disk being read from.
 * @param buf		Buffer to read into.
 * @param lba		Block number to read.
 * @return		Whether reading succeeded. */
static bool partition_block_read(disk_t *disk, void *buf, offset_t lba) {
	disk_t *parent = disk->data;
	return parent->ops->block_read(parent, buf, lba + disk->offset);
}

/** Operations for a partition disk. */
static disk_ops_t partition_disk_ops = {
	.block_read = partition_block_read,
};

/** Add a partition to a disk device.
 * @param disk		Disk to add to.
 * @param id		ID of partition.
 * @param lba		Block number that the partitions starts at.
 * @param blocks	Number of blocks the partition takes up. */
void disk_partition_add(disk_t *disk, int id, offset_t lba, file_size_t blocks) {
	disk_t *child = kmalloc(sizeof(disk_t));
	vfs_filesystem_t *fs;

	child->id = id;
	child->blksize = disk->blksize;
	child->blocks = blocks;
	child->ops = &partition_disk_ops;
	child->data = disk;
	child->partial_block = NULL;
	child->boot = false;
	child->offset = lba;

	if((fs = vfs_filesystem_probe(child))) {
		if(disk->boot && disk->ops->is_boot_partition) {
			if(disk->ops->is_boot_partition(child, id, lba)) {
				boot_filesystem = fs;
			}
		}
	} else {
		kfree(child);
	}
}

/** Add a disk device.
 * @param id		ID of the disk (specific to the implementation).
 * @param blksize	Size of a block on the disk.
 * @param blocks	Number of blocks to read.
 * @param ops		Pointer to operations structure.
 * @param data		Implementation-specific data pointer.
 * @param boot		Whether the disk is the boot device.
 * @return		Pointer to disk structure, or NULL if disk contained
 *			nothing usable. */
disk_t *disk_add(uint8_t id, size_t blksize, file_size_t blocks, disk_ops_t *ops, void *data, bool boot) {
	disk_t *disk = kmalloc(sizeof(disk_t));
	vfs_filesystem_t *fs;
	size_t i;

	disk->id = id;
	disk->blksize = blksize;
	disk->blocks = blocks;
	disk->ops = ops;
	disk->data = data;
	disk->partial_block = NULL;
	disk->boot = boot;
	disk->offset = 0;

	/* Probe the disk for filesystems/partitions. */
	if((fs = vfs_filesystem_probe(disk))) {
		if(boot) {
			boot_filesystem = fs;
		}
		return disk;
	} else {
		for(i = 0; i < ARRAYSZ(partition_probe_funcs); i++) {
			if(partition_probe_funcs[i](disk)) {
				return disk;
			}
		}

		kfree(disk);
		return NULL;
	}
}

/** Initialise the disk system. */
void disk_init(void) {
	platform_disk_detect();
	if(!boot_filesystem) {
		fatal("Could not find boot filesystem");
	}
}
