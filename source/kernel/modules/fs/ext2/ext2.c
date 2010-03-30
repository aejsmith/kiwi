/*
 * Copyright (C) 2008-2010 Alex Smith
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
 * @brief		Ext2 filesystem module.
 */

#include <io/device.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <errors.h>
#include <module.h>
#include <time.h>

#include "ext2_priv.h"

/** Clean up data associated with an Ext2 node.
 * @param node		Node to clean up. */
static void ext2_node_free(fs_node_t *node) {
	ext2_inode_release(node->data);
}

/** Write to an Ext2 file.
 * @note		This function merely reserves disk space and updates
 *			modification times. The actual write is done using the
 *			page cache.
 * @todo		Allocate space!
 * @param node		Node to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset into file to write to.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes written.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_write(fs_node_t *node, const void *buf, size_t count, offset_t offset,
                           bool nonblock, size_t *bytesp) {
	ext2_inode_t *inode = node->data;

	inode->disk.i_mtime = USECS2SECS(time_since_epoch());
	ext2_inode_flush(inode);
	return 0;
}

/** Read a page of data from an Ext2 file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param offset	Offset within the file to read from (multiple of
 *			PAGE_SIZE).
 * @param nonblock	Whether the read is required to not block.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_read_page(fs_node_t *node, void *buf, offset_t offset, bool nonblock) {
	ext2_inode_t *inode = node->data;
	int ret;

	assert(node->type == FS_NODE_FILE);
	assert(inode->mount->block_size <= PAGE_SIZE);
	assert(!(offset % inode->mount->block_size));

	rwlock_read_lock(&inode->lock);
	ret = ext2_inode_read(inode, buf, offset / inode->mount->block_size,
	                      PAGE_SIZE / inode->mount->block_size, nonblock);
	rwlock_unlock(&inode->lock);
	return (ret < 0) ? ret : 0;
}

/** Write a page of data to an Ext2 file.
 * @param node		Node to write to.
 * @param buf		Buffer containing data to write.
 * @param offset	Offset within the file to write to (multiple of
 *			PAGE_SIZE).
 * @param nonblock	Whether the write is required to not block.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_write_page(fs_node_t *node, const void *buf, offset_t offset, bool nonblock) {
	ext2_inode_t *inode = node->data;
	int ret;

	assert(node->type == FS_NODE_FILE);
	assert(inode->mount->block_size <= PAGE_SIZE);
	assert(!(offset % inode->mount->block_size));

	rwlock_write_lock(&inode->lock);
	ret = ext2_inode_write(inode, buf, offset / inode->mount->block_size,
	                       PAGE_SIZE / inode->mount->block_size, nonblock);
	rwlock_unlock(&inode->lock);
	return (ret < 0) ? ret : 0;
}

/** Modify the size of an Ext2 file.
 * @param node		Node being resized.
 * @param size		New size of the node.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_resize(fs_node_t *node, offset_t size) {
	ext2_inode_t *inode = node->data;
	int ret;

	rwlock_write_lock(&inode->lock);
	ret = ext2_inode_resize(inode, size);
	rwlock_unlock(&inode->lock);
	return ret;
}

/** Create a new node as a child of an existing directory.
 * @param _parent	Directory to create in.
 * @param name		Name to give directory entry.
 * @param node		Node structure describing the node being
 *			created. For symbolic links, the link_dest
 *			pointer in the node will point to a string
 *			containing the link destination.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_create(fs_node_t *_parent, const char *name, fs_node_t *node) {
	ext2_inode_t *parent = _parent->data, *inode;
	int ret, count;
	uint16_t mode;
	size_t len;
	void *buf;

	/* Work out the mode. */
	mode = (node->type == FS_NODE_DIR) ? 0755 : 0644;
	switch(node->type) {
	case FS_NODE_FILE:
		mode |= EXT2_S_IFREG;
		break;
	case FS_NODE_DIR:
		mode |= EXT2_S_IFDIR;
		break;
	case FS_NODE_SYMLINK:
		mode |= EXT2_S_IFLNK;
		break;
	default:
		return -ERR_NOT_SUPPORTED;
	}

	/* Allocate the inode. Use the parent's UID/GID for now. */
	if((ret = ext2_inode_alloc(parent->mount, mode, parent->disk.i_uid,
	                           parent->disk.i_gid, &inode)) != 0) {
		return ret;
	}
	rwlock_write_lock(&inode->lock);
	rwlock_write_lock(&parent->lock);

	/* Fill in node structure. */
	node->id = inode->num;
	node->data = inode;

	/* Add the . and .. entries when creating a directory, and fill in
	 * link destination when creating a symbolic link. */
	if(node->type == FS_NODE_DIR) {
		if((ret = ext2_dir_insert(inode, inode, ".")) != 0) {
			goto fail;
		} else if((ret = ext2_dir_insert(inode, parent, "..")) != 0) {
			goto fail;
		}
	} else if(node->type == FS_NODE_SYMLINK) {
		assert(node->link_cache);
		len = strlen(node->link_cache);

		inode->disk.i_size = cpu_to_le32(len);
		if(len <= sizeof(inode->disk.i_block)) {
			memcpy(inode->disk.i_block, node->link_cache, len);
			//inode->disk.i_mtime = time_get_current();
		} else {
			buf = kmalloc(inode->mount->block_size, MM_SLEEP);
			memcpy(buf, node->link_cache, len);
			count = ROUND_UP(len, inode->mount->block_size) / inode->mount->block_size;
			if((ret = ext2_inode_write(inode, buf, 0, count, false)) != count) {
				ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
				kfree(buf);
				goto fail;
			}
			kfree(buf);
		}
	}

	/* Add entry to parent. */
	if((ret = ext2_dir_insert(parent, inode, name)) != 0) {
		goto fail;
	}

	rwlock_unlock(&inode->lock);
	rwlock_unlock(&parent->lock);
	return 0;
fail:
	rwlock_unlock(&parent->lock);
	rwlock_unlock(&inode->lock);
	ext2_inode_release(inode);
	return ret;
}

/** Remove an entry from an Ext2 directory.
 * @param _parent	Directory containing the node.
 * @param name		Name of the node in the directory.
 * @param node		Node being unlinked.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_unlink(fs_node_t *_parent, const char *name, fs_node_t *node) {
	ext2_inode_t *parent = _parent->data, *inode = node->data;
	int ret;

	assert(parent);
	assert(inode);

	rwlock_write_lock(&parent->lock);
	rwlock_write_lock(&inode->lock);

	if(node->type == FS_NODE_DIR) {
		/* Remove the . and .. entries. The VFS ensures that these are
		 * the only entries in the directory. */
		if((ret = ext2_dir_remove(inode, inode, ".")) != 0) {
			rwlock_unlock(&inode->lock);
			rwlock_unlock(&parent->lock);
			return ret;
		} else if((ret = ext2_dir_remove(inode, parent, "..")) != 0) {
			rwlock_unlock(&inode->lock);
			rwlock_unlock(&parent->lock);
			return ret;
		}
	}

	/* This will decrease link counts as required. The actual removal
	 * will take place when the ext2_node_free() is called on the node. */
	if((ret = ext2_dir_remove(parent, inode, name)) == 0) {
		if(le16_to_cpu(inode->disk.i_links_count) == 0) {
			fs_node_remove(node);
		}
	}

	rwlock_unlock(&inode->lock);
	rwlock_unlock(&parent->lock);
	return ret;
}

/** Get information about a node.
 * @param node		Node to get information on.
 * @param info		Information structure to fill in. */
static void ext2_node_info(fs_node_t *node, fs_info_t *info) {
	ext2_inode_t *inode = node->data;

	rwlock_read_lock(&inode->lock);
	info->size = le32_to_cpu(inode->disk.i_size);
	info->links = le16_to_cpu(inode->disk.i_links_count);
	rwlock_unlock(&inode->lock);
}

/** Cache directory contents.
 * @param node		Node to cache contents of.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_cache_children(fs_node_t *node) {
	return ext2_dir_cache(node);
}

/** Store the destination of a symbolic link.
 * @param node		Symbolic link to cache destination of.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_cache_dest(fs_node_t *node) {
	ext2_inode_t *inode = node->data;
	char *buf, *tmp;
	int ret, count;
	size_t size;

	rwlock_read_lock(&inode->lock);

	size = le32_to_cpu(inode->disk.i_size);
	if(le32_to_cpu(inode->disk.i_blocks) == 0) {
		buf = kmalloc(size + 1, MM_SLEEP);
		memcpy(buf, inode->disk.i_block, size);
		buf[size] = 0;
	} else {
		if(!(buf = kmalloc(ROUND_UP(size, inode->mount->block_size) + 1, 0))) {
			rwlock_unlock(&inode->lock);
			return -ERR_NO_MEMORY;
		}

		count = ROUND_UP(size, inode->mount->block_size) / inode->mount->block_size;
		if((ret = ext2_inode_read(inode, buf, 0, count, false)) != count) {
			kfree(buf);
			rwlock_unlock(&inode->lock);
			return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
		}

		buf[size] = 0;
		if(!(tmp = krealloc(buf, size + 1, 0))) {
			kfree(buf);
			rwlock_unlock(&inode->lock);
			return -ERR_NO_MEMORY;
		}
		buf = tmp;
	}

	rwlock_unlock(&inode->lock);
	node->link_cache = buf;
	return 0;
}

/** Ext2 node operations structure. */
static fs_node_ops_t ext2_node_ops = {
	.free = ext2_node_free,
	.write = ext2_node_write,
	.read_page = ext2_node_read_page,
	.write_page = ext2_node_write_page,
	.resize = ext2_node_resize,
	.create = ext2_node_create,
	.unlink = ext2_node_unlink,
	.info = ext2_node_info,
	.cache_children = ext2_node_cache_children,
	.cache_dest = ext2_node_cache_dest,
};

/** Flush data for an Ext2 mount to disk.
 * @note		Should not be called if mount is read-only.
 * @note		Mount should be write locked.
 * @param mount         Mount to flush. */
void ext2_mount_flush(ext2_mount_t *mount) {
	size_t bytes;
	int ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	ret = device_write(mount->device, &mount->sb, sizeof(ext2_superblock_t), 1024, &bytes);
	if(ret != 0 || bytes != sizeof(ext2_superblock_t)) {
		kprintf(LOG_WARN, "ext2: warning: could not write back superblock during flush (%d, %zu)\n",
		        ret, bytes);
	}

	ret = device_write(mount->device, mount->group_tbl, mount->group_tbl_size, mount->group_tbl_offset, &bytes);
	if(ret != 0 || bytes != mount->group_tbl_size) {
		kprintf(LOG_WARN, "ext2: warning: could not write back group table during flush (%d, %zu)\n",
		        ret, bytes);
	}
}

/** Unmount an Ext2 filesystem.
 * @param mount		Mount to unmount. */
static void ext2_unmount(fs_mount_t *mount) {
	ext2_mount_t *data = mount->data;

	if(!(mount->flags & FS_MOUNT_RDONLY)) {
		data->sb.s_state = cpu_to_le16(EXT2_VALID_FS);
		ext2_mount_flush(data);
	}
}

/** Read in an Ext2 filesystem node.
 * @param mount		Mount to get node from.
 * @param id		ID of node to get.
 * @param nodep		Where to store pointer to node structure.
 * @return		0 on success, negative error code on failure. */
static int ext2_read_node(fs_mount_t *mount, node_id_t id, fs_node_t **nodep) {
	ext2_mount_t *data = mount->data;
	fs_node_type_t type;
	ext2_inode_t *inode;
	int ret;

	if((ret = ext2_inode_get(data, id, &inode)) != 0) {
		return ret;
	}

	/* Figure out the node type. */
	switch(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFMT) {
	case EXT2_S_IFSOCK:
		type = FS_NODE_SOCK;
		break;
	case EXT2_S_IFLNK:
		type = FS_NODE_SYMLINK;
		break;
	case EXT2_S_IFREG:
		type = FS_NODE_FILE;
		break;
	case EXT2_S_IFBLK:
		type = FS_NODE_BLKDEV;
		break;
	case EXT2_S_IFDIR:
		type = FS_NODE_DIR;
		break;
	case EXT2_S_IFCHR:
		type = FS_NODE_CHRDEV;
		break;
	case EXT2_S_IFIFO:
		type = FS_NODE_FIFO;
		break;
	default:
		dprintf("ext2: inode %" PRIu64 " has invalid type in mode (%" PRIu16 ")\n",
		        id, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return -ERR_FORMAT_INVAL;
	}

	/* Sanity check. */
	if(id == EXT2_ROOT_INO && type != FS_NODE_DIR) {
		dprintf("ext2: root inode %" PRIu64 " is not a directory (%" PRIu16 ")\n",
		        id, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return -ERR_FORMAT_INVAL;
	}

	/* Create and fill out a node structure. */
	*nodep = fs_node_alloc(mount, id, type, &ext2_node_ops, inode);
	return 0;
}

/** Ext2 mount operations structure. */
static fs_mount_ops_t ext2_mount_ops = {
	.unmount = ext2_unmount,
	.read_node = ext2_read_node,
};

/** Check whether a device contains an Ext2 filesystem.
 * @param handle	Handle to device to check.
 * @param uuid		If not NULL, UUID to check for.
 * @return		Whether if the device contains an Ext2 FS with the
 *			given UUID. */
static bool ext2_probe(object_handle_t *handle, const char *uuid) {
	ext2_superblock_t *sb;
	uint32_t revision;
	size_t bytes;

	sb = kmalloc(sizeof(ext2_superblock_t), MM_SLEEP);
	if(device_read(handle, sb, sizeof(ext2_superblock_t), 1024, &bytes) != 0) {
		kfree(sb);
		return false;
	} else if(bytes != sizeof(ext2_superblock_t) || le16_to_cpu(sb->s_magic) != EXT2_MAGIC) {
		kfree(sb);
		return false;
	}

	/* Check if the revision is supported. */
	revision = le32_to_cpu(sb->s_rev_level);
	if(revision != EXT2_GOOD_OLD_REV && revision != EXT2_DYNAMIC_REV) {
		dprintf("ext2: device %s has unknown revision %" PRIu32 "\n",
		        device_name(handle), revision);
		kfree(sb);
		return false;
	}

	/* Check for incompatible features. */
	if(EXT2_HAS_INCOMPAT_FEATURE(sb, ~(EXT2_FEATURE_INCOMPAT_RO_SUPP | EXT2_FEATURE_INCOMPAT_SUPP))) {
		dprintf("ext2: device %s has unsupported incompatible features %u\n",
		        device_name(handle), sb->s_feature_incompat);
		kfree(sb);
		return false;
	}

	kfree(sb);
	return true;
}

/** Mount an Ext2 filesystem.
 * @param mount		Mount structure for the FS.
 * @param opts		Array of options passed to the mount call.
 * @param count		Number of options in the array.
 * @return		0 on success, negative error code on failure. */
static int ext2_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
	ext2_mount_t *data;
	size_t bytes;
	int ret = 0;

	/* Create a mount structure to track information about the mount. */
	mount->ops = &ext2_mount_ops;
	data = mount->data = kcalloc(1, sizeof(ext2_mount_t), MM_SLEEP);
	rwlock_init(&data->lock, "ext2_mount_lock");
	data->parent = mount;
	data->device = mount->device;

	/* Read in the superblock. Note that ext2_probe() will have been called
	 * so the device will contain a supported filesystem. */
	if((ret = device_read(data->device, &data->sb, sizeof(ext2_superblock_t), 1024, &bytes)) != 0) {
		goto fail;
	} else if(bytes != sizeof(ext2_superblock_t)) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* If not mounting read-only, check for read-only features, and whether
	 * the FS is clean. */
	if(!(mount->flags & FS_MOUNT_RDONLY)) {
		if(EXT2_HAS_RO_COMPAT_FEATURE(&data->sb, ~EXT2_FEATURE_RO_COMPAT_SUPP) ||
		   EXT2_HAS_INCOMPAT_FEATURE(&data->sb, EXT2_FEATURE_INCOMPAT_RO_SUPP)) {
			kprintf(LOG_WARN, "ext2: %s has unsupported write features, mounting read-only\n",
			        device_name(data->device));
			mount->flags |= FS_MOUNT_RDONLY;
		} else if(le16_to_cpu(data->sb.s_state) != EXT2_VALID_FS) {
			kprintf(LOG_WARN, "ext2: warning: %s not cleanly unmounted/damaged, mounting read-only\n",
			        device_name(data->device));
			mount->flags |= FS_MOUNT_RDONLY;
		}
	}

	/* Get useful information out of the superblock. */
	data->revision = le32_to_cpu(data->sb.s_rev_level);
	data->inodes_per_group = le32_to_cpu(data->sb.s_inodes_per_group);
	data->inode_count = le32_to_cpu(data->sb.s_inodes_count);
	data->blocks_per_group = le32_to_cpu(data->sb.s_blocks_per_group);
	data->block_count = le32_to_cpu(data->sb.s_blocks_count);
	data->block_size = 1024 << le32_to_cpu(data->sb.s_log_block_size);
	if(data->block_size > PAGE_SIZE) {
		kprintf(LOG_WARN, "ext2: cannot support block size greater than system page size!\n");
		ret = -ERR_NOT_SUPPORTED;
		goto fail;
	}
	data->block_groups = data->inode_count / data->inodes_per_group;
	data->inode_size = (data->revision == EXT2_DYNAMIC_REV) ? le16_to_cpu(data->sb.s_inode_size) : 128;
	data->group_tbl_offset = data->block_size * (le32_to_cpu(data->sb.s_first_data_block) + 1);
	data->group_tbl_size = ROUND_UP(data->block_groups * sizeof(ext2_group_desc_t), data->block_size);

	dprintf("ext2: mounting ext2 filesystem from device %s...\n", device_name(data->device));
	dprintf(" revision:     %u\n", data->revision);
	dprintf(" block_size:   %u\n", data->block_size);
	dprintf(" block_groups: %u\n", data->block_groups);
	dprintf(" inode_size:   %u\n", data->inode_size);
	dprintf(" block_count:  %u\n", data->block_count);
	dprintf(" inode_count:  %u\n", data->inode_count);

	/* Read in the group descriptor table. Don't use MM_SLEEP as it could
	 * be very big. */
	if(!(data->group_tbl = kmalloc(data->group_tbl_size, 0))) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	} else if((ret = device_read(data->device, data->group_tbl, data->group_tbl_size,
	                             data->group_tbl_offset, &bytes)) != 0) {
		dprintf("ext2: failed to read in group table for %s (%d)\n",
		        device_name(data->device), ret);
		goto fail;
	} else if(bytes != data->group_tbl_size) {
		dprintf("ext2: incorrect size returned when reading group table for %s (%zu, wanted %zu)\n",
		        device_name(data->device), bytes, data->group_tbl_size);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* If mounting read-write, write back the superblock as mounted. */
	if(!(mount->flags & FS_MOUNT_RDONLY)) {
		data->sb.s_state = cpu_to_le16(EXT2_ERROR_FS);
		data->sb.s_mnt_count = cpu_to_le16(le16_to_cpu(data->sb.s_mnt_count) + 1);

		if((ret = device_write(data->device, &data->sb, sizeof(ext2_superblock_t), 1024, &bytes)) != 0) {
			goto fail;
		} else if(bytes != sizeof(ext2_superblock_t)) {
			ret = -ERR_DEVICE_ERROR;
			goto fail;
		}
	}

	/* Now get the root inode (second inode in first group descriptor) */
	if((ret = ext2_read_node(mount, EXT2_ROOT_INO, &mount->root)) != 0) {
		goto fail;
	}

	dprintf("ext2: mounted device %s (data: %p)\n", device_name(data->device), data);
	return 0;
fail:
	if(data->group_tbl) {
		kfree(data->group_tbl);
	}
	kfree(data);
	return ret;
}

/** Ext2 filesystem type structure. */
static fs_type_t ext2_fs_type = {
	.name = "ext2",
	.description = "Second Extended Filesystem",
	.probe = ext2_probe,
	.mount = ext2_mount,
};

/** Initialisation function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static int ext2_init(void) {
	return fs_type_register(&ext2_fs_type);
}

/** Unloading function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static int ext2_unload(void) {
	return fs_type_unregister(&ext2_fs_type);
}

MODULE_NAME("ext2");
MODULE_DESC("Ext2 filesystem module");
MODULE_FUNCS(ext2_init, ext2_unload);
