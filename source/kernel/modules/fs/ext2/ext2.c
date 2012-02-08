/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED AS IS AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/**
 * @file
 * @brief		Ext2 filesystem module.
 */

#include <io/device.h>

#include <lib/string.h>

#include <mm/malloc.h>

#include <assert.h>
#include <module.h>
#include <status.h>
#include <time.h>

#include "ext2_priv.h"

/** Clean up data associated with an Ext2 node.
 * @param node		Node to clean up. */
static void ext2_node_free(fs_node_t *node) {
	ext2_inode_release(node->data);
}

/** Flush changes to an Ext2 node.
 * @param node		Node to flush.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_flush(fs_node_t *node) {
	ext2_inode_t *inode = node->data;
	status_t ret;

	ret = vm_cache_flush(inode->cache);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	return ext2_inode_flush(inode);
}

/** Create a new node as a child of an existing directory.
 * @param _parent	Directory to create in.
 * @param name		Name to give directory entry.
 * @param type		Type to give the new node.
 * @param security	Security attributes for the node.
 * @param target	For symbolic links, the target of the link.
 * @param nodep		Where to store pointer to node for created entry.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_create(fs_node_t *_parent, const char *name, file_type_t type,
                                 const char *target, object_security_t *security,
                                 fs_node_t **nodep) {
	ext2_inode_t *parent = _parent->data, *inode;
	size_t len, bytes;
	uint16_t mode = 0;
	status_t ret;

	/* Work out the mode for the type. */
	switch(type) {
	case FILE_TYPE_REGULAR:
		mode |= EXT2_S_IFREG;
		break;
	case FILE_TYPE_DIR:
		mode |= EXT2_S_IFDIR;
		break;
	case FILE_TYPE_SYMLINK:
		mode |= EXT2_S_IFLNK;
		break;
	default:
		return STATUS_NOT_SUPPORTED;
	}

	/* Allocate the inode. Use the parent's UID/GID for now. */
	ret = ext2_inode_alloc(parent->mount, mode, security, &inode);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	mutex_lock(&parent->lock);

	/* Add the . and .. entries when creating a directory, and fill in
	 * link destination when creating a symbolic link. */
	if(type == FILE_TYPE_DIR) {
		ret = ext2_dir_insert(inode, ".", inode);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}

		ret = ext2_dir_insert(inode, "..", parent);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		}
	} else if(type == FILE_TYPE_SYMLINK) {
		len = strlen(target);
		if(len <= sizeof(inode->disk.i_block)) {
			inode->size = len;
			memcpy(inode->disk.i_block, target, inode->size);
			inode->disk.i_mtime = le32_to_cpu(USECS2SECS(unix_time()));
			ext2_inode_flush(inode);
		} else {
			ret = ext2_inode_write(inode, target, len, 0, false, &bytes);
			if(ret != STATUS_SUCCESS) {
				goto fail;
			} else if(bytes != len) {
				ret = STATUS_CORRUPT_FS;
				goto fail;
			}
		}
	}

	/* Add entry to parent. */
	ret = ext2_dir_insert(parent, name, inode);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	mutex_unlock(&parent->lock);
	*nodep = fs_node_alloc(_parent->mount, inode->num, type, security, _parent->ops, inode);
	return STATUS_SUCCESS;
fail:
	mutex_unlock(&parent->lock);
	ext2_inode_release(inode);
	return ret;
}

/** Remove an entry from an Ext2 directory.
 * @param _parent	Directory containing the node.
 * @param name		Name of the node in the directory.
 * @param node		Node being unlinked.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_unlink(fs_node_t *_parent, const char *name, fs_node_t *node) {
	ext2_inode_t *parent = _parent->data, *inode = node->data;
	status_t ret;

	mutex_lock(&parent->lock);
	mutex_lock(&inode->lock);

	if(node->type == FILE_TYPE_DIR) {
		/* Ensure that it's empty. */
		if(!ext2_dir_empty(inode)) {
			ret = STATUS_DIR_NOT_EMPTY;
			goto out;
		}

		/* Remove the . and .. entries. */
		ret = ext2_dir_remove(inode, ".", inode);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
		ret = ext2_dir_remove(inode, "..", parent);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}
	}

	/* This will decrease link counts as required. The actual removal
	 * will take place when the ext2_node_free() is called on the node. */
	ret = ext2_dir_remove(parent, name, inode);
	if(ret == STATUS_SUCCESS) {
		if(le16_to_cpu(inode->disk.i_links_count) == 0) {
			fs_node_remove(node);
		}
	}
out:
	mutex_unlock(&inode->lock);
	mutex_unlock(&parent->lock);
	return ret;
}

/** Get information about an Ext2 node.
 * @param node		Node to get information on.
 * @param infop		Information structure to fill in. */
static void ext2_node_info(fs_node_t *node, file_info_t *infop) {
	ext2_inode_t *inode = node->data;

	mutex_lock(&inode->lock);

	infop->block_size = PAGE_SIZE;
	infop->size = inode->size;
	infop->links = le16_to_cpu(inode->disk.i_links_count);
	infop->created = SECS2USECS(le32_to_cpu(inode->disk.i_ctime));
	infop->accessed = SECS2USECS(le32_to_cpu(inode->disk.i_atime));
	infop->modified = SECS2USECS(le32_to_cpu(inode->disk.i_mtime));

	mutex_unlock(&inode->lock);
}

/** Update security attributes of an Ext2 node.
 * @param node		Node to set for.
 * @param security	New security attributes to set.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_set_security(fs_node_t *node, const object_security_t *security) {
	ext2_inode_t *inode = node->data;
	return ext2_inode_set_security(inode, security);
}

/** Read from an Ext2 file.
 * @param node		Node to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into file to read from.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes read.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_read(fs_node_t *node, void *buf, size_t count, offset_t offset,
                               bool nonblock, size_t *bytesp) {
	ext2_inode_t *inode = node->data;
	return ext2_inode_read(inode, buf, count, offset, nonblock, bytesp);
}

/** Write to an Ext2 file.
 * @param node		Node to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset into file to write to.
 * @param nonblock	Whether the write is required to not block.
 * @param bytesp	Where to store number of bytes written.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_write(fs_node_t *node, const void *buf, size_t count, offset_t offset,
                                bool nonblock, size_t *bytesp) {
	ext2_inode_t *inode = node->data;
	return ext2_inode_write(inode, buf, count, offset, nonblock, bytesp);
}

/** Get the data cache for an Ext2 file.
 * @param node		Node to get cache for.
 * @return		Pointer to node's VM cache. */
static vm_cache_t *ext2_node_get_cache(fs_node_t *node) {
	ext2_inode_t *inode = node->data;
	return inode->cache;
}

/** Modify the size of an Ext2 file.
 * @param node		Node being resized.
 * @param size		New size of the node.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_resize(fs_node_t *node, offset_t size) {
	ext2_inode_t *inode = node->data;
	return ext2_inode_resize(inode, size);
}

/** Directory iteration callback for reading a directory entry.
 * @param dir		Directory being iterated.
 * @param header	Pointer to directory entry header.
 * @param name		Name of entry.
 * @param offset	Offset of entry.
 * @param data		Where to store pointer to entry structure.
 * @return		Always returns false. */
static bool ext2_read_entry_cb(ext2_inode_t *dir, ext2_dirent_t *header, const char *name,
                               offset_t offset, void *data) {
	dir_entry_t *entry;
	size_t len;

	len = sizeof(*entry) + strlen(name) + 1;
	entry = kmalloc(len, MM_SLEEP);
	entry->length = len;
	entry->id = le32_to_cpu(header->inode);
	strcpy(entry->name, name);

	*(dir_entry_t **)data = entry;
	return false;
}

/** Read an Ext2 directory entry.
 * @param node		Node to read from.
 * @param index		Index of entry to read.
 * @param entryp	Where to store pointer to directory entry structure.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_read_entry(fs_node_t *node, offset_t index, dir_entry_t **entryp) {
	ext2_inode_t *inode = node->data;
	return ext2_dir_iterate(inode, index, ext2_read_entry_cb, entryp);
}

/** Look up an Ext2 directory entry.
 * @param node		Node to look up in.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry points to.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_lookup_entry(fs_node_t *node, const char *name, node_id_t *idp) {
	ext2_inode_t *inode = node->data;
	return entry_cache_lookup(inode->entries, name, idp);
}

/** Read the destination of an Ext2 symbolic link.
 * @param node		Node to read from.
 * @param destp		Where to store pointer to string containing link
 *			destination.
 * @return		Status code describing result of the operation. */
static status_t ext2_node_read_link(fs_node_t *node, char **destp) {
	ext2_inode_t *inode = node->data;
	status_t ret;
	size_t bytes;
	char *dest;

	dest = kmalloc(inode->size + 1, MM_SLEEP);
	if(le32_to_cpu(inode->disk.i_blocks) == 0) {
		memcpy(dest, inode->disk.i_block, inode->size);
	} else {
		ret = ext2_inode_read(inode, dest, inode->size, 0, false, &bytes);
		if(ret != STATUS_SUCCESS) {
			kfree(dest);
			return ret;
		} else if(bytes != inode->size) {
			kfree(dest);
			return STATUS_CORRUPT_FS;
		}
	}

	dest[inode->size] = 0;
	*destp = dest;
	return STATUS_SUCCESS;
}

/** Ext2 node operations structure. */
static fs_node_ops_t ext2_node_ops = {
	.free = ext2_node_free,
	.flush = ext2_node_flush,
	.create = ext2_node_create,
	.unlink = ext2_node_unlink,
	.info = ext2_node_info,
	.set_security = ext2_node_set_security,
	.read = ext2_node_read,
	.write = ext2_node_write,
	.get_cache = ext2_node_get_cache,
	.resize = ext2_node_resize,
	.read_entry = ext2_node_read_entry,
	.lookup_entry = ext2_node_lookup_entry,
	.read_link = ext2_node_read_link,
};

/** Flush data for an Ext2 mount to disk.
 * @note		Should not be called if mount is read-only.
 * @note		Mount should be write locked.
 * @param mount         Mount to flush. */
void ext2_mount_flush(ext2_mount_t *mount) {
	status_t ret;
	size_t bytes;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	ret = device_write(mount->device, &mount->sb, sizeof(ext2_superblock_t), 1024, &bytes);
	if(ret != STATUS_SUCCESS || bytes != sizeof(ext2_superblock_t)) {
		kprintf(LOG_WARN, "ext2: warning: could not write back superblock during flush (%d, %zu)\n",
		        ret, bytes);
	}

	ret = device_write(mount->device, mount->group_tbl, mount->group_tbl_size, mount->group_tbl_offset, &bytes);
	if(ret != STATUS_SUCCESS || bytes != mount->group_tbl_size) {
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
 * @return		Status code describing result of the operation. */
static status_t ext2_read_node(fs_mount_t *mount, node_id_t id, fs_node_t **nodep) {
	ext2_mount_t *data = mount->data;
	object_security_t *security;
	ext2_inode_t *inode;
	file_type_t type;
	status_t ret;

	ret = ext2_inode_get(data, id, &inode);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	/* Figure out the node type. */
	switch(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFMT) {
	case EXT2_S_IFSOCK:
		type = FILE_TYPE_SOCK;
		break;
	case EXT2_S_IFLNK:
		type = FILE_TYPE_SYMLINK;
		break;
	case EXT2_S_IFREG:
		type = FILE_TYPE_REGULAR;
		break;
	case EXT2_S_IFBLK:
		type = FILE_TYPE_BLKDEV;
		break;
	case EXT2_S_IFDIR:
		type = FILE_TYPE_DIR;
		break;
	case EXT2_S_IFCHR:
		type = FILE_TYPE_CHRDEV;
		break;
	case EXT2_S_IFIFO:
		type = FILE_TYPE_FIFO;
		break;
	default:
		dprintf("ext2: inode %" PRIu32 " has invalid type in mode (%" PRIu16 ")\n",
		        inode->num, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return STATUS_CORRUPT_FS;
	}

	/* Sanity check. */
	if(id == EXT2_ROOT_INO && type != FILE_TYPE_DIR) {
		dprintf("ext2: root inode %" PRIu32 " is not a directory (%" PRIu16 ")\n",
		        inode->num, le16_to_cpu(inode->disk.i_mode));
		ext2_inode_release(inode);
		return STATUS_CORRUPT_FS;
	}

	/* Get security attributes for the node. */
	ret = ext2_inode_security(inode, &security);
	if(ret != STATUS_SUCCESS) {
		ext2_inode_release(inode);
		return ret;
	}

	/* Create and fill out a node structure. */
	*nodep = fs_node_alloc(mount, id, type, security, &ext2_node_ops, inode);
	object_security_destroy(security);
	return STATUS_SUCCESS;
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
	char *tmp;

	sb = kmalloc(sizeof(ext2_superblock_t), MM_SLEEP);
	if(device_read(handle, sb, sizeof(ext2_superblock_t), 1024, &bytes) != STATUS_SUCCESS) {
		kfree(sb);
		return false;
	} else if(bytes != sizeof(ext2_superblock_t) || le16_to_cpu(sb->s_magic) != EXT2_MAGIC) {
		kfree(sb);
		return false;
	}

	/* Check if the revision is supported. We require DYNAMIC_REV for UUID
	 * support. */
	revision = le32_to_cpu(sb->s_rev_level);
	if(revision != EXT2_DYNAMIC_REV) {
		dprintf("ext2: device %s has unsupported revision %" PRIu32 "\n",
		        device_name(handle), revision);
		kfree(sb);
		return false;
	}

	/* Check for incompatible features. */
	if(EXT2_HAS_INCOMPAT_FEATURE(sb, ~EXT2_FEATURE_INCOMPAT_SUPP)) {
		dprintf("ext2: device %s has unsupported incompatible features %u\n",
		        device_name(handle), sb->s_feature_incompat);
		kfree(sb);
		return false;
	}

	/* Check the UUID if required. */
	if(uuid) {
		tmp = kmalloc(37, MM_SLEEP);
		sprintf(tmp, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		        sb->s_uuid[0], sb->s_uuid[1], sb->s_uuid[2], sb->s_uuid[3], sb->s_uuid[4],
		        sb->s_uuid[5], sb->s_uuid[6], sb->s_uuid[7], sb->s_uuid[8], sb->s_uuid[9],
		        sb->s_uuid[10], sb->s_uuid[11], sb->s_uuid[12], sb->s_uuid[13], sb->s_uuid[14],
		        sb->s_uuid[15]);
		if(strcmp(tmp, uuid) != 0) {
			kfree(tmp);
			kfree(sb);
			return false;
		}
		kfree(tmp);
	}

	kfree(sb);
	return true;
}

/** Mount an Ext2 filesystem.
 * @param mount		Mount structure for the FS.
 * @param opts		Array of options passed to the mount call.
 * @param count		Number of options in the array.
 * @return		Status code describing result of the operation. */
static status_t ext2_mount(fs_mount_t *mount, fs_mount_option_t *opts, size_t count) {
	ext2_mount_t *data;
	status_t ret;
	size_t bytes;

	/* Create a mount structure to track information about the mount. */
	mount->ops = &ext2_mount_ops;
	data = mount->data = kcalloc(1, sizeof(ext2_mount_t), MM_SLEEP);
	mutex_init(&data->lock, "ext2_mount_lock", 0);
	data->parent = mount;
	data->device = mount->device;

	/* Read in the superblock. Note that ext2_probe() will have been called
	 * so the device will contain a supported filesystem. */
	ret = device_read(data->device, &data->sb, sizeof(ext2_superblock_t), 1024, &bytes);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	} else if(bytes != sizeof(ext2_superblock_t)) {
		ret = STATUS_CORRUPT_FS;
		goto fail;
	}

	/* If not mounting read-only, check for read-only features, and whether
	 * the FS is clean. */
	if(!(mount->flags & FS_MOUNT_RDONLY)) {
		if(EXT2_HAS_RO_COMPAT_FEATURE(&data->sb, ~EXT2_FEATURE_RO_COMPAT_SUPP)) {
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
		ret = STATUS_NOT_SUPPORTED;
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
	data->group_tbl = kmalloc(data->group_tbl_size, 0);
	if(!data->group_tbl) {
		ret = STATUS_NO_MEMORY;
		goto fail;
	}

	ret = device_read(data->device, data->group_tbl, data->group_tbl_size,
	                  data->group_tbl_offset, &bytes);
	if(ret != STATUS_SUCCESS) {
		dprintf("ext2: failed to read in group table for %s (%d)\n",
		        device_name(data->device), ret);
		goto fail;
	} else if(bytes != data->group_tbl_size) {
		dprintf("ext2: incorrect size returned when reading group table for %s (%zu, wanted %zu)\n",
		        device_name(data->device), bytes, data->group_tbl_size);
		ret = STATUS_CORRUPT_FS;
		goto fail;
	}

	/* If mounting read-write, write back the superblock as mounted. */
	if(!(mount->flags & FS_MOUNT_RDONLY)) {
		data->sb.s_state = cpu_to_le16(EXT2_ERROR_FS);
		data->sb.s_mnt_count = cpu_to_le16(le16_to_cpu(data->sb.s_mnt_count) + 1);

		ret = device_write(data->device, &data->sb, sizeof(ext2_superblock_t), 1024, &bytes);
		if(ret != STATUS_SUCCESS) {
			goto fail;
		} else if(bytes != sizeof(ext2_superblock_t)) {
			ret = STATUS_CORRUPT_FS;
			goto fail;
		}
	}

	/* Now get the root inode (second inode in first group descriptor) */
	ret = ext2_read_node(mount, EXT2_ROOT_INO, &mount->root);
	if(ret != STATUS_SUCCESS) {
		goto fail;
	}

	dprintf("ext2: mounted device %s (data: %p)\n", device_name(data->device), data);
	return STATUS_SUCCESS;
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

/** Initialization function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static status_t ext2_init(void) {
	return fs_type_register(&ext2_fs_type);
}

/** Unloading function for the Ext2 module.
 * @return		0 on success, negative error code on failure. */
static status_t ext2_unload(void) {
	return fs_type_unregister(&ext2_fs_type);
}

MODULE_NAME("ext2");
MODULE_DESC("Ext2 filesystem module");
MODULE_FUNCS(ext2_init, ext2_unload);
