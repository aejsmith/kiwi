/* Kiwi Ext2 filesystem module
 * Copyright (C) 2008-2009 Alex Smith
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

#include <lib/string.h>
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <endian.h>
#include <errors.h>
#include <module.h>

#include "ext2_priv.h"

static int ext2_node_get(vfs_node_t *node, identifier_t id);

/** Check whether a device contains an Ext2 filesystem.
 * @param device	Device to check.
 * @return		Whether if the device contains an Ext2 FS. */
static bool ext2_probe(device_t *device) {
	ext2_superblock_t *sb = kmalloc(sizeof(ext2_superblock_t), MM_SLEEP);
	uint32_t revision;
	bool found = true;
	size_t bytes;
	int ret;

	if((ret = device_read(device, sb, sizeof(ext2_superblock_t), 1024, &bytes)) != 0) {
		found = false;
	} else if(bytes != sizeof(ext2_superblock_t) || le16_to_cpu(sb->s_magic) != EXT2_MAGIC) {
		found = false;
	}

	revision = le32_to_cpu(sb->s_rev_level);
	if(revision != EXT2_GOOD_OLD_REV && revision != EXT2_DYNAMIC_REV) {
		dprintf("ext2: device %p(%s) has unknown revision %" PRIu32 "\n", device,
		        device->name, revision);
		found = false;
	}

	kfree(sb);
	return found;
}

/** Mount an Ext2 filesystem.
 * @param _mount	Mount structure for the FS.
 * @return		0 on success, negative error code on failure. */
static int ext2_mount(vfs_mount_t *_mount) {
	ext2_mount_t *mount;
	uint32_t revision;
	size_t bytes;
	int ret = 0;

	/* Create a mount structure to track information about the mount. */
	mount = _mount->data = kcalloc(1, sizeof(ext2_mount_t), MM_SLEEP);
	rwlock_init(&mount->lock, "ext2_mount_lock");
	mount->parent = _mount;
	mount->device = _mount->device;

	/* Read in the superblock. Must recheck whether we support it as
	 * something could change between probe and this function. */
	if((ret = device_read(mount->device, &mount->sb, sizeof(ext2_superblock_t), 1024, &bytes)) != 0) {
		goto fail;
	} else if(bytes != sizeof(ext2_superblock_t) || le16_to_cpu(mount->sb.s_magic) != EXT2_MAGIC) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	revision = le32_to_cpu(mount->sb.s_rev_level);
	if(revision != EXT2_GOOD_OLD_REV && revision != EXT2_DYNAMIC_REV) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Print a warning if the FS is not clean and mount it read-only. */
	if(le16_to_cpu(mount->sb.s_state) != EXT2_VALID_FS) {
		kprintf(LOG_WARN, "ext2: warning: %s not cleanly unmounted/damaged, mounting read only\n",
		        mount->device->name);
		_mount->flags |= VFS_MOUNT_RDONLY;
	}

	/* Get useful information out of the superblock. */
	mount->inodes_per_group = le32_to_cpu(mount->sb.s_inodes_per_group);
	mount->inodes_count = le32_to_cpu(mount->sb.s_inodes_count);
	mount->blocks_per_group = le32_to_cpu(mount->sb.s_blocks_per_group);
	mount->blocks_count = le32_to_cpu(mount->sb.s_blocks_count);
	mount->blk_size = 1024 << le32_to_cpu(mount->sb.s_log_block_size);
	if(mount->blk_size > PAGE_SIZE) {
		dprintf("ext2: cannot support block size greater than system page size!\n");
		ret = -ERR_NOT_SUPPORTED;
		goto fail;
	}
	mount->blk_groups = mount->inodes_count / mount->inodes_per_group;
	mount->in_size = (le32_to_cpu(mount->sb.s_rev_level) == EXT2_DYNAMIC_REV) ? le16_to_cpu(mount->sb.s_inode_size) : 128;
	mount->group_tbl_off = mount->blk_size * (le32_to_cpu(mount->sb.s_first_data_block) + 1);
	mount->group_tbl_size = ROUND_UP(mount->blk_groups * sizeof(ext2_group_desc_t), mount->blk_size);

	dprintf("ext2: mounting ext2 filesystem from device %p(%s)...\n", mount->device, mount->device->name);
	dprintf(" rev_level:  %u\n", le32_to_cpu(mount->sb.s_rev_level));
	dprintf(" blk_size:   %u\n", mount->blk_size);
	dprintf(" blk_groups: %u\n", mount->blk_groups);
	dprintf(" in_size:    %u\n", mount->in_size);
	dprintf(" blocks:     %u\n", mount->blocks_count);

	/* Read in the group descriptor table. Don't use MM_SLEEP as it could
	 * be very big. */
	mount->group_tbl = kmalloc(mount->group_tbl_size, 0);
	if(!mount->group_tbl) {
		ret = -ERR_NO_MEMORY;
		goto fail;
	} else if((ret = device_read(mount->device, mount->group_tbl, mount->group_tbl_size, mount->group_tbl_off, &bytes)) != 0) {
		dprintf("ext2: failed to read in group table (%d)\n", ret);
		goto fail;
	} else if(bytes != mount->group_tbl_size) {
		dprintf("ext2: incorrect size returned when reading group table (%zu, wanted %zu)\n",
		        bytes, mount->group_tbl_size);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* If mounting read-write, write back the superblock as mounted. */
	if(!(_mount->flags & VFS_MOUNT_RDONLY)) {
		mount->sb.s_state = cpu_to_le16(EXT2_ERROR_FS);
		mount->sb.s_mnt_count = cpu_to_le16(le16_to_cpu(mount->sb.s_mnt_count) + 1);

		if((ret = device_write(mount->device, &mount->sb, sizeof(ext2_superblock_t), 1024, &bytes)) != 0) {
			goto fail;
		} else if(bytes != sizeof(ext2_superblock_t)) {
			ret = -ERR_DEVICE_ERROR;
			goto fail;
		}
	}

	/* Now get the root inode (second inode in first group descriptor) */
	if((ret = ext2_node_get(_mount->root, EXT2_ROOT_INO)) != 0) {
		goto fail;
	}

	dprintf("ext2: mounted device %p(%s) mounted (mount: %p)\n", mount->device,
		mount->device->name, mount);
	return 0;
fail:
	if(mount->group_tbl) {
		kfree(mount->group_tbl);
	}
	kfree(mount);
	return ret;
}

/** Unmount an Ext2 filesystem.
 * @param mount		Mount being unmounted.
 * @return		0 on success, negative error code on failure. */
static int ext2_unmount(vfs_mount_t *mount) {
	return -ERR_NOT_IMPLEMENTED;
}

/** Read a page of data from an Ext2 node.
 * @todo		Nonblocking.
 * @param node		Node to read data read from.
 * @param page		Pointer to mapped page to read into.
 * @param offset	Offset within the file to read from (multiple of
 *			PAGE_SIZE).
 * @param nonblock	Whether the read is required to not block.
 * @return		0 on success, negative error code on failure. */
static int ext2_page_read(vfs_node_t *node, void *page, offset_t offset, bool nonblock) {
	ext2_inode_t *inode = node->data;
	int ret;

	assert(node->type == VFS_NODE_FILE);
	assert(inode->mount->blk_size <= PAGE_SIZE);
	assert(!(offset % inode->mount->blk_size));

	ret = ext2_inode_read(inode, page, offset / inode->mount->blk_size, PAGE_SIZE / inode->mount->blk_size);
	return (ret < 0) ? ret : 0;
}

/** Read in an Ext2 filesystem node.
 * @param node		Node structure to fill out.
 * @param id		ID of node that structure is for.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_get(vfs_node_t *node, identifier_t id) {
	ext2_mount_t *mount = node->mount->data;
	ext2_inode_t *inode;
	int ret;

	if((ret = ext2_inode_get(mount, id, &inode)) != 0) {
		return ret;
	}

	node->id = id;
	node->data = inode;
	node->size = le32_to_cpu(inode->disk.i_size);

	/* Figure out the node type. */
	switch(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFMT) {
	case EXT2_S_IFSOCK:
		node->type = VFS_NODE_SOCK;
		break;
	case EXT2_S_IFLNK:
		node->type = VFS_NODE_SYMLINK;
		break;
	case EXT2_S_IFREG:
		node->type = VFS_NODE_FILE;
		break;
	case EXT2_S_IFBLK:
		node->type = VFS_NODE_BLKDEV;
		break;
	case EXT2_S_IFDIR:
		/* For directories size is used internally by the VFS. */
		node->size = 0;
		node->type = VFS_NODE_DIR;
		break;
	case EXT2_S_IFCHR:
		node->type = VFS_NODE_CHRDEV;
		break;
	case EXT2_S_IFIFO:
		node->type = VFS_NODE_FIFO;
		break;
	default:
		dprintf("ext2: inode %" PRId32 " has invalid type in mode (%" PRIu16 ")\n",
		        id, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return -ERR_FORMAT_INVAL;
	}

	/* Sanity check. */
	if(id == EXT2_ROOT_INO && node->type != VFS_NODE_DIR) {
		dprintf("ext2: root inode %" PRId32 " is not a directory (%" PRIu16 ")\n",
		        id, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return -ERR_FORMAT_INVAL;
	}

	return 0;
}

/** Flush changes to an Ext2 node.
 * @param node		Node to flush.
 * @return		0 on success, negative error code on failure. */
static int ext2_node_flush(vfs_node_t *node) {
	return ext2_inode_flush(node->data);
}

/** Clean up data associated with a node structure.
 * @todo		Remove if link count 0.
 * @note		This can only be called from vfs_node_free() which
 *			holds the node/mount locks between calling flush and
 *			this function - this guarantees that the inode won't
 *			be dirty when we get here.
 * @param node		Node to clean up. */
static void ext2_node_free(vfs_node_t *node) {
	ext2_inode_release(node->data);
}

/** Get the destination of a symbolic link.
 * @param node		Symbolic link to get destination of.
 * @param bufp		Where to store pointer to string containing
 *			link destination.
 * @return		0 on success, negative error code on failure. */
static int ext2_symlink_read(vfs_node_t *node, char **bufp) {
	ext2_inode_t *inode = (ext2_inode_t *)node->data;
	char *buf, *tmp;
	int ret, count;
	size_t size;

	/* Ok to lock it here despite the fact that ext2_inode_read() does:
	 * multiple read locks can be done on a rwlock by a thread. */
	rwlock_read_lock(&inode->lock, 0);

	size = le32_to_cpu(inode->disk.i_size);
	if(le32_to_cpu(inode->disk.i_blocks) == 0) {
		buf = kmalloc(size + 1, MM_SLEEP);
		memcpy(buf, inode->disk.i_block, size);
		buf[size] = 0;
	} else {
		if(!(buf = kmalloc(ROUND_UP(size, inode->mount->blk_size) + 1, 0))) {
			rwlock_unlock(&inode->lock);
			return -ERR_NO_MEMORY;
		}

		count = ROUND_UP(size, inode->mount->blk_size) / inode->mount->blk_size;
		if((ret = ext2_inode_read(inode, buf, 0, count)) != count) {
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
	}

	rwlock_unlock(&inode->lock);
	*bufp = buf;
	return 0;
}

/** Ext2 filesystem type structure. */
static vfs_type_t ext2_fs_type = {
	.name = "ext2",
	.flags = VFS_TYPE_RDONLY,
	.probe = ext2_probe,
	.mount = ext2_mount,
	.unmount = ext2_unmount,
	.page_read = ext2_page_read,
	.node_get = ext2_node_get,
	.node_flush = ext2_node_flush,
	.node_free = ext2_node_free,
	.dir_cache = ext2_dir_cache,
	.symlink_read = ext2_symlink_read,
};

/** Initialization function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static int ext2_init(void) {
	return vfs_type_register(&ext2_fs_type);
}

/** Unloading function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static int ext2_unload(void) {
	return vfs_type_unregister(&ext2_fs_type);
}

MODULE_NAME("ext2");
MODULE_DESC("Ext2 filesystem module");
MODULE_FUNCS(ext2_init, ext2_unload);
