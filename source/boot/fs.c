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

#include <boot/fs.h>
#include <boot/memory.h>

#include <lib/string.h>

#include <assert.h>

#include "fs/ext2.h"
#include "fs/iso9660.h"

/** List of all detected filesystems. */
LIST_DECLARE(filesystem_list);

/** The filesystem being booted from. */
fs_mount_t *boot_filesystem = NULL;

/** Boot path override string. */
char *boot_path_override = NULL;

/** Array of paths to search for boot files. */
static const char *boot_paths[] = {
	"/system/boot",
	"/kiwi",
};

/** Array of filesystem implementations. */
static fs_type_t *filesystem_types[] = {
	&ext2_fs_type,
	&iso9660_fs_type,
};

/** Look up a node by ID on a filesystem.
 * @param mount		Mount to get from.
 * @param id		ID of node to get.
 * @return		Pointer to node or NULL if not found. */
static fs_node_t *fs_node_read(fs_mount_t *mount, node_id_t id) {
	fs_node_t *node;

	/* Search in the node cache first. */
	LIST_FOREACH(&mount->nodes, iter) {
		node = list_entry(iter, fs_node_t, header);
		if(node->id == id) {
			fs_node_get(node);
			return node;
		}
	}

	/* Try to read the node from the filesystem. */
	if(!(node = mount->type->read_node(mount, id))) {
		return NULL;
	}

	/* Cache the retreived node. */
	list_append(&mount->nodes, &node->header);
	return node;
}

/** Allocate a new node structure.
 * @param mount		Mount that the node resides on.
 * @param id		ID of the node.
 * @param type		Type of the node.
 * @param size		Size of the file data (if a file).
 * @param data		Implementation-specific data pointer.
 * @return		Pointer to node structure. */
fs_node_t *fs_node_alloc(fs_mount_t *mount, node_id_t id, int type, offset_t size, void *data) {
	fs_node_t *node = kmalloc(sizeof(fs_node_t));

	list_init(&node->header);
	refcount_set(&node->count, 1);
	list_init(&node->entries);
	node->mount = mount;
	node->id = id;
	node->type = type;
	node->size = size;
	node->data = data;
	return node;
}

/** Increase the reference count of a node.
 * @param node		Node to increase count of. */
void fs_node_get(fs_node_t *node) {
	node->count++;
}

/** Decrease the reference count of a node.
 * @param node		Node to decrease count of. */
void fs_node_release(fs_node_t *node) {
	assert(node->count > 0);
	if(--node->count == 0) {
		/* FIXME: Don't actually do anything right now, as kfree() is
		 * not implemented - if we free things, we'll actually end up
		 * running out of heap quicker. */
	}
}

/** Look up a path on a filesystem.
 * @param mount		Mount to look up on.
 * @param path		Path to look up.
 * @return		Pointer to node on success, NULL on failure. */
fs_node_t *fs_lookup(fs_mount_t *mount, const char *path) {
	fs_node_t *node, *child;
	char *dup, *tok;

	assert(path[0] == '/');
	while(path[0] == '/') {
		path++;
	}

	node = mount->root;
	fs_node_get(node);

	/* Loop through each element of the path string. The string must be
	 * duplicated so that it can be modified. */
	dup = kstrdup(path);
	while(true) {
		tok = strsep(&dup, "/");
		if(tok == NULL) {
			/* The last token was the last element of the path
			 * string, return the node we're currently on. */
			return node;
		} else if(node->type != FS_NODE_DIR) {
			/* The previous node was not a directory: this means
			 * the path string is trying to treat a non-directory
			 * as a directory. Reject this. */
			fs_node_release(node);
			return NULL;
		} else if(!tok[0]) {
			/* Zero-length path component, do nothing. */
			continue;
		}

		/* Get the entry from the directory. */
		if(!(child = fs_dir_lookup(node, tok))) {
			fs_node_release(node);
			return NULL;
		}

		fs_node_release(node);
		node = child;
	}
}

/** Check whether a boot path exists on a filesystem.
 * @param mount		Mount to check.
 * @param path		Path to look for.
 * @return		Node for directory, or NULL if doesn't exist. */
static fs_node_t *check_boot_path(fs_mount_t *mount, const char *path) {
	fs_node_t *node;

	if((node = fs_lookup(mount, path))) {
		if(node->type == FS_NODE_DIR) {
			return node;
		}
		fs_node_release(node);
	}

	return NULL;
}

/** Get the node referring to the boot directory.
 * @param mount		Filesystem to get from.
 * @return		Node for boot directory, or NULL if not bootable. */
fs_node_t *fs_find_boot_path(fs_mount_t *mount) {
	fs_node_t *node;
	size_t i;

	if(boot_path_override) {
		return check_boot_path(mount, boot_path_override);
	} else {
		for(i = 0; i < ARRAYSZ(boot_paths); i++) {
			if((node = check_boot_path(mount, boot_paths[i]))) {
				return node;
			}
		}

		return NULL;
	}
}

/** Read from a file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset in the file to read from.
 * @return		Whether the read was successful. */
bool fs_file_read(fs_node_t *node, void *buf, size_t count, offset_t offset) {
	assert(node->type == FS_NODE_FILE);
	return node->mount->type->read_file(node, buf, count, offset);
}

/** Insert an entry into a directory.
 * @param node		Node to add to.
 * @param name		Name of the entry to add (will be duplicated).
 * @param id		ID of the node the entry refers to. */
void fs_dir_insert(fs_node_t *node, char *name, node_id_t id) {
	fs_dir_entry_t *entry;

	assert(node->type == FS_NODE_DIR);

	entry = kmalloc(sizeof(fs_dir_entry_t));
	list_init(&entry->header);
	entry->name = kstrdup(name);
	entry->id = id;

	list_append(&node->entries, &entry->header);
}

/** Look up an entry in a directory.
 * @param node		Directory to look up in.
 * @param name		Name of entry to look up.
 * @return		Pointer to node on success, NULL on failure. */
fs_node_t *fs_dir_lookup(fs_node_t *node, const char *name) {
	fs_dir_entry_t *entry;
	fs_node_t *child;

	if(list_empty(&node->entries)) {
		if(!node->mount->type->read_dir(node)) {
			return NULL;
		}
	}

	LIST_FOREACH(&node->entries, iter) {
		entry = list_entry(iter, fs_dir_entry_t, header);
		if(strcmp(entry->name, name) == 0) {
			if(!(child = fs_node_read(node->mount, entry->id))) {
				return NULL;
			}
			return child;
		}
	}

	return NULL;
}

/** Iterate through entries in a directory.
 * @param node		Node for directory.
 * @param prev		Previous entry (or NULL to start from beginning).
 * @return		Pointer to entry structure for next entry. */
fs_dir_entry_t *fs_dir_iterate(fs_node_t *node, fs_dir_entry_t *prev) {
	assert(node->type == FS_NODE_DIR);

	if(list_empty(&node->entries)) {
		if(!node->mount->type->read_dir(node)) {
			return NULL;
		} else if(list_empty(&node->entries)) {
			return NULL;
		}
	}

	if(prev) {
		if(&prev->header == node->entries.prev) {
			return NULL;
		}
		return list_entry(prev->header.next, fs_dir_entry_t, header);
	} else {
		return list_entry(node->entries.next, fs_dir_entry_t, header);
	}
}

/** Probe a disk for filesystems.
 * @param disk		Disk to probe.
 * @return		Pointer to mount if detected, NULL if not. */
static fs_mount_t *disk_probe(disk_t *disk) {
	fs_mount_t *mount;
	fs_node_t *node;
	size_t i;

	mount = kmalloc(sizeof(fs_mount_t));
	list_init(&mount->header);
	list_init(&mount->nodes);
	mount->disk = disk;

	for(i = 0; i < ARRAYSZ(filesystem_types); i++) {
		mount->type = filesystem_types[i];
		if(!mount->type->mount(mount)) {
			continue;
		}

		/* Check if bootable. */
		if(!(node = fs_find_boot_path(mount))) {
			break;
		}
		fs_node_release(node);

		list_append(&filesystem_list, &mount->header);
		return mount;
	}

	kfree(mount);
	return NULL;
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
		if(!disk->ops->read_block(disk, disk->partial_block, start)) {
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
		if(!disk->ops->read_block(disk, buf, start)) {
			return false;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!disk->ops->read_block(disk, disk->partial_block, start)) {
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
static bool partition_read_block(disk_t *disk, void *buf, uint64_t lba) {
	disk_t *parent = disk->data;
	return parent->ops->read_block(parent, buf, lba + disk->offset);
}

/** Operations for a partition disk. */
static disk_ops_t partition_disk_ops = {
	.read_block = partition_read_block,
};

/** Add a partition to a disk device.
 * @param disk		Disk to add to.
 * @param id		ID of partition.
 * @param lba		Block number that the partitions starts at.
 * @param blocks	Number of blocks the partition takes up. */
void disk_partition_add(disk_t *disk, int id, uint64_t lba, uint64_t blocks) {
	disk_t *child = kmalloc(sizeof(disk_t));
	fs_mount_t *mount;

	child->id = id;
	child->blksize = disk->blksize;
	child->blocks = blocks;
	child->ops = &partition_disk_ops;
	child->data = disk;
	child->partial_block = NULL;
	child->boot = false;
	child->offset = lba;

	if((mount = disk_probe(child))) {
		if(disk->boot && disk->ops->is_boot_partition) {
			if(disk->ops->is_boot_partition(child, id, lba)) {
				boot_filesystem = mount;
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
disk_t *disk_add(uint8_t id, size_t blksize, uint64_t blocks, disk_ops_t *ops, void *data, bool boot) {
	disk_t *disk = kmalloc(sizeof(disk_t));
	fs_mount_t *mount;
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
	if((mount = disk_probe(disk))) {
		if(boot) {
			boot_filesystem = mount;
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
