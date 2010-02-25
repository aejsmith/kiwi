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
 * @brief		Ext2 filesystem support.
 */

#include <boot/console.h>
#include <boot/memory.h>

#include <lib/string.h>

#include <assert.h>
#include <endian.h>

#include "ext2.h"

/** Data for an Ext2 mount. */
typedef struct ext2_mount {
	ext2_superblock_t sb;			/**< Superblock of the filesystem. */
	ext2_group_desc_t *group_tbl;		/**< Pointer to block group descriptor table. */
	vfs_filesystem_t *parent;		/**< Pointer to FS structure. */
	uint32_t inodes_per_group;		/**< Inodes per group. */
	uint32_t inodes_count;			/**< Inodes count. */
	size_t blk_size;			/**< Size of a block on the filesystem. */
	size_t blk_groups;			/**< Number of block groups. */
	size_t in_size;				/**< Size of an inode. */
	void *temp_block1;			/**< Temporary block 1. */
	void *temp_block2;			/**< Temporary block 2. */
} ext2_mount_t;

/** In-memory inode structure. */
typedef struct ext2_inode {
	ext2_mount_t *mount;			/**< Pointer to mount data structure. */
	ext2_disk_inode_t disk;			/**< On-disk inode structure. */
} ext2_inode_t;

/** Read a block from an Ext2 filesystem.
 * @param mount		Mount to read from.
 * @param buf		Buffer to read into.
 * @param num		Block number.
 * @return		Whether successful. */
static bool ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t num) {
	return disk_read(mount->parent->disk, buf, mount->blk_size, (uint64_t)num * mount->blk_size);
}

/** Recurse through the extent index tree to find a leaf.
 * @param mount		Mount being read from.
 * @param header	Extent header to start at.
 * @param block		Block number to get.
 * @return		Pointer to header for leaf, NULL on failure. */
static ext4_extent_header_t *ext4_find_leaf(ext2_mount_t *mount, ext4_extent_header_t *header,
                                            uint32_t block) {
	ext4_extent_idx_t *index;
	uint16_t i;

	while(true) {
		index = (ext4_extent_idx_t *)&header[1];

		if(le16_to_cpu(header->eh_magic) != EXT4_EXT_MAGIC) {
			return NULL;
		} else if(!le16_to_cpu(header->eh_depth)) {
			return header;
		}

		for(i = 0; i < le16_to_cpu(header->eh_entries); i++) {
			if(block < le32_to_cpu(index[i].ei_block)) {
				break;
			}
		}

		if(!i) {
			return NULL;
		} else if(!mount->temp_block1) {
			mount->temp_block1 = kmalloc(mount->blk_size);
		}

		if(!ext2_block_read(mount, mount->temp_block1, le32_to_cpu(index[i - 1].ei_leaf))) {
			return NULL;
		}
		header = (ext4_extent_header_t *)mount->temp_block1;
	}
}

/** Get the raw block number from an inode block number.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to get block for.
 * @param block		Block number within the inode to get.
 * @param nump		Where to store raw block number.
 * @return		Whether successful. */
static bool ext2_inode_block_get(ext2_inode_t *inode, uint32_t block, uint32_t *nump) {
	uint32_t *i_block, *bi_block, num;
	ext4_extent_header_t *header;
	ext4_extent_t *extent;
	uint16_t i;

	if(le32_to_cpu(inode->disk.i_flags) & EXT4_EXTENTS_FL) {
		header = ext4_find_leaf(inode->mount, (ext4_extent_header_t *)inode->disk.i_block, block);
		if(!header) {
			return false;
		}

		extent = (ext4_extent_t *)&header[1];
		for(i = 0; i < le16_to_cpu(header->eh_entries); i++) {
			if(block < le32_to_cpu(extent[i].ee_block)) {
				break;
			}
		}

		if(!i) {
			return false;
		}

		block -= le32_to_cpu(extent[i - 1].ee_block);
		if(block >= le16_to_cpu(extent[i - 1].ee_len)) {
			*nump = 0;
		} else {
			*nump = block + le32_to_cpu(extent[i - 1].ee_start);
		}

		return true;
	} else {
		/* First check if it's a direct block. This is easy to handle,
		 * just need to get it straight out of the inode structure. */
		if(block < EXT2_NDIR_BLOCKS) {
			*nump = le32_to_cpu(inode->disk.i_block[block]);
			return true;
		}

		block -= EXT2_NDIR_BLOCKS;
		if(!inode->mount->temp_block1) {
			inode->mount->temp_block1 = kmalloc(inode->mount->blk_size);
		}
		i_block = inode->mount->temp_block1;

		/* Check whether the indirect block contains the block number
		 * we need. The indirect block contains as many 32-bit entries
		 * as will fit in one block of the filesystem. */
		if(block < (inode->mount->blk_size / sizeof(uint32_t))) {
			num = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
			if(num == 0) {
				*nump = 0;
				return true;
			} else if(!ext2_block_read(inode->mount, i_block, num)) {
				return false;
			}

			*nump = le32_to_cpu(i_block[block]);
			return true;
		}

		block -= inode->mount->blk_size / sizeof(uint32_t);
		if(!inode->mount->temp_block2) {
			inode->mount->temp_block2 = kmalloc(inode->mount->blk_size);
		}
		bi_block = inode->mount->temp_block2;

		/* Not in the indirect block, check the bi-indirect blocks. The
		 * bi-indirect block contains as many 32-bit entries as will
		 * fit in one block of the filesystem, with each entry pointing
		 * to an indirect block. */
		if(block < ((inode->mount->blk_size / sizeof(uint32_t)) * (inode->mount->blk_size / sizeof(uint32_t)))) {
			num = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK]);
			if(num == 0) {
				*nump = 0;
				return true;
			} else if(!ext2_block_read(inode->mount, bi_block, num)) {
				return false;
			}

			/* Get indirect block inside bi-indirect block. */
			num = le32_to_cpu(bi_block[block / (inode->mount->blk_size / sizeof(uint32_t))]);
			if(num == 0) {
				*nump = 0;
				return true;
			} else if(!ext2_block_read(inode->mount, i_block, num)) {
				return false;
			}

			*nump = le32_to_cpu(i_block[block % (inode->mount->blk_size / sizeof(uint32_t))]);
			return true;
		}

		/* Triple indirect block. I somewhat doubt this will be needed
		 * in the bootloader. */
		dprintf("ext2: tri-indirect blocks not yet supported!\n");
		return false;
	}
}

/** Read blocks from an Ext2 inode.
 * @param inode		Inode to read from.
 * @param buf		Buffer to read into.
 * @param block		Starting block number.
 * @return		Whether read successfully. */
static bool ext2_inode_block_read(ext2_inode_t *inode, void *buf, uint32_t block) {
	uint32_t raw = 0;

	if(block >= ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->blk_size) / inode->mount->blk_size) {
		return false;
	} else if(!ext2_inode_block_get(inode, block, &raw)) {
		return false;
	}

	/* If the block number is 0, then it's a sparse block. */
	if(raw == 0) {
		memset(buf, 0, inode->mount->blk_size);
		return true;
	} else {
		return ext2_block_read(inode->mount, buf, raw);
	}
}

/** Read a node from the filesystem.
 * @param fs		Filesystem to read from.
 * @param id		ID of node.
 * @return		Pointer to node on success, NULL on failure. */
static vfs_node_t *ext2_node_get(vfs_filesystem_t *fs, inode_t id) {
	ext2_mount_t *mount = fs->data;
	ext2_inode_t *inode;
	size_t group, size;
	offset_t offset;
	int type;

	/* Get the group descriptor table containing the inode. */
	if((group = (id - 1) / mount->inodes_per_group) >= mount->blk_groups) {
		dprintf("ext2: bad inode number %llu\n", id);
		return NULL;
	}

	/* Get the offset of the inode in the group's inode table. */
	offset = ((id - 1) % mount->inodes_per_group) * mount->in_size;

	/* Create a structure to store details of the inode in memory. */
	inode = kmalloc(sizeof(ext2_inode_t));
	inode->mount = mount;

	/* Read it in. */
	size = (mount->in_size <= sizeof(ext2_disk_inode_t)) ? mount->in_size : sizeof(ext2_disk_inode_t);
	offset = ((offset_t)le32_to_cpu(mount->group_tbl[group].bg_inode_table) * mount->blk_size) + offset;
	if(!disk_read(fs->disk, &inode->disk, size, offset)) {
		dprintf("ext2: failed to read inode %llu\n", id);
		kfree(inode);
		return false;
	}

	size = le32_to_cpu(inode->disk.i_size);
	switch(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFMT) {
	case EXT2_S_IFREG:
		type = VFS_NODE_FILE;
		break;
	case EXT2_S_IFDIR:
		/* For directories size is used internally by the VFS. */
		size = 0;
		type = VFS_NODE_DIR;
		break;
	default:
		dprintf("ext2: unhandled inode type for %llu\n", id);
		kfree(inode);
		return false;
	}

	return vfs_node_alloc(fs, id, type, size, inode);
}

/** Read from a file.
 * @param node		Node referring to file.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into the file.
 * @return		Whether read successfully. */
static bool ext2_file_read(vfs_node_t *node, void *buf, size_t count, offset_t offset) {
	ext2_mount_t *mount = node->fs->data;
	ext2_inode_t *inode = node->data;
	size_t blksize = mount->blk_size;
	uint32_t start, end, i, size;

	/* Allocate a temporary buffer for partial transfers if required. */
	if((offset % blksize || count % blksize) && !mount->temp_block1) {
		mount->temp_block1 = kmalloc(blksize);
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
		if(!ext2_inode_block_read(inode, mount->temp_block1, start)) {
			return false;
		}

		size = (start == end) ? count : blksize - (size_t)(offset % blksize);
		memcpy(buf, mount->temp_block1 + (offset % blksize), size);
		buf += size; count -= size; start++;
	}

	/* Handle any full blocks. */
	size = count / blksize;
	for(i = 0; i < size; i++, buf += blksize, count -= blksize, start++) {
		/* Read directly into the destination buffer. */
		if(!ext2_inode_block_read(inode, buf, start)) {
			return false;
		}
	}

	/* Handle anything that's left. */
	if(count > 0) {
		if(!ext2_inode_block_read(inode, mount->temp_block1, start)) {
			return false;
		}

		memcpy(buf, mount->temp_block1, count);
	}

	return true;
}

/** Cache directory entries.
 * @param node		Node to cache entries from.
 * @return		Whether cached successfully. */
static bool ext2_dir_cache(vfs_node_t *node) {
	ext2_inode_t *inode = node->data;
	char *buf = NULL, *name = NULL;
	ext2_dirent_t *dirent;
	uint32_t current = 0;
	bool ret = false;

	/* Allocate buffers to read the data into. */
	buf = kmalloc(le32_to_cpu(inode->disk.i_size));
	name = kmalloc(EXT2_NAME_MAX + 1);

	/* Read in all the directory entries required. */
	if(!ext2_file_read(node, buf, le32_to_cpu(inode->disk.i_size), 0)) {
		goto out;
	}

	while(current < le32_to_cpu(inode->disk.i_size)) {
		dirent = (ext2_dirent_t *)(buf + current);
		current += le16_to_cpu(dirent->rec_len);

		if(dirent->file_type != EXT2_FT_UNKNOWN && dirent->name_len != 0) {
			strncpy(name, dirent->name, dirent->name_len);
			name[dirent->name_len] = 0;
			dprintf("'%s' %u\n", name, dirent->inode);
			vfs_dir_insert(node, name, le32_to_cpu(dirent->inode));
		} else if(!le16_to_cpu(dirent->rec_len)) {
			break;
		}
	}

	ret = true;
out:
	if(buf) {
		kfree(buf);
	}
	if(name) {
		kfree(name);
	}
	return ret;
}

/** Create an instance of an Ext2 filesystem.
 * @param fs		Filesystem structure to fill in.
 * @return		Whether succeeded in mounting. */
static bool ext2_mount(vfs_filesystem_t *fs) {
	ext2_mount_t *mount;
	offset_t offset;
	size_t size;

	/* Create a mount structure to track information about the mount. */
	mount = fs->data = kmalloc(sizeof(ext2_mount_t));
	mount->parent = fs;

	/* Read in the superblock. Must recheck whether we support it as
	 * something could change between probe and this function. */
	if(!disk_read(fs->disk, &mount->sb, sizeof(ext2_superblock_t), 1024)) {
		goto fail;
	} else if(le16_to_cpu(mount->sb.s_magic) != EXT2_MAGIC) {
		goto fail;
	} else if(le32_to_cpu(mount->sb.s_rev_level) != EXT2_DYNAMIC_REV) {
		/* Have to reject this because GOOD_OLD_REV does not have
		 * a UUID or label. */
		dprintf("ext2: not EXT2_DYNAMIC_REV!\n");
		goto fail;
	}

	/* Get useful information out of the superblock. */
	mount->inodes_per_group = le32_to_cpu(mount->sb.s_inodes_per_group);
	mount->inodes_count = le32_to_cpu(mount->sb.s_inodes_count);
	mount->blk_size = 1024 << le32_to_cpu(mount->sb.s_log_block_size);
	mount->blk_groups = mount->inodes_count / mount->inodes_per_group;
	mount->in_size = le16_to_cpu(mount->sb.s_inode_size);
	mount->temp_block1 = NULL;
	mount->temp_block2 = NULL;

	/* Read in the group descriptor table. */
	offset = mount->blk_size * (le32_to_cpu(mount->sb.s_first_data_block) + 1);
	size = ROUND_UP(mount->blk_groups * sizeof(ext2_group_desc_t), mount->blk_size);
	mount->group_tbl = kmalloc(size);
	if(!disk_read(fs->disk, mount->group_tbl, size, offset)) {
		goto fail;
	}

	/* Now get the root inode (second inode in first group descriptor) */
	if(!(fs->root = ext2_node_get(fs, EXT2_ROOT_INO))) {
		goto fail;
	}

	/* Store label and UUID. */
	fs->label = kstrdup(mount->sb.s_volume_name);
	fs->uuid = kmalloc(37);
	sprintf(fs->uuid, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
	         mount->sb.s_uuid[0], mount->sb.s_uuid[1], mount->sb.s_uuid[2],
	         mount->sb.s_uuid[3], mount->sb.s_uuid[4], mount->sb.s_uuid[5],
	         mount->sb.s_uuid[6], mount->sb.s_uuid[7], mount->sb.s_uuid[8],
	         mount->sb.s_uuid[9], mount->sb.s_uuid[10], mount->sb.s_uuid[11],
	         mount->sb.s_uuid[12], mount->sb.s_uuid[13], mount->sb.s_uuid[14],
	         mount->sb.s_uuid[15]);

	if(fs->disk->offset) {
		dprintf("ext2: disk 0x%x partition %u mounted (label: %s, uuid: %s)\n",
		        fs->disk->parent->id, fs->disk->id, fs->label, fs->uuid);
	} else {
		dprintf("ext2: disk 0x%x mounted (label: %s, uuid: %s)\n",
		        fs->disk->id, fs->label, fs->uuid);
	}
	return true;
fail:
	kfree(mount);
	return false;
}

/** Ext2 filesystem operations structure. */
vfs_filesystem_ops_t ext2_filesystem_ops = {
	.node_get = ext2_node_get,
	.file_read = ext2_file_read,
	.dir_cache = ext2_dir_cache,
	.mount = ext2_mount,
};
