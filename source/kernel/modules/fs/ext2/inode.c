/*
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

#include "ext2_priv.h"

/** Get an inode from an Ext2 filesystem.
 * @param mount		Mount to read from. Should be locked.
 * @param num		Inode number to read.
 * @param inodep	Where to store pointer to inode structure.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_get_internal(ext2_mount_t *mount, uint32_t num, ext2_inode_t **inodep) {
	ext2_inode_t *inode = NULL;
	size_t group, bytes;
	offset_t offset;
	int ret;

	/* Get the group descriptor table containing the inode. */
	if((group = (num - 1) / mount->inodes_per_group) >= mount->blk_groups) {
		dprintf("ext2: group number %zu is invalid on mount %p\n", group, mount);
		return -ERR_FORMAT_INVAL;
	}

	/* Get the offset of the inode in the group's inode table. */
	offset = ((num - 1) % mount->inodes_per_group) * mount->in_size;

	/* Create a structure to store details of the inode in memory. */
	inode = kmalloc(sizeof(ext2_inode_t), MM_SLEEP);
	rwlock_init(&inode->lock, "ext2_inode_lock");
	inode->dirty = false;
	inode->num = num;
	inode->size = (mount->in_size <= sizeof(ext2_disk_inode_t)) ? mount->in_size : sizeof(ext2_disk_inode_t);
	inode->offset = ((offset_t)le32_to_cpu(mount->group_tbl[group].bg_inode_table) * mount->blk_size) + offset;
	inode->mount = mount;

	/* Read it in. */
	if((ret = device_read(mount->device, &inode->disk, inode->size, inode->offset, &bytes)) != 0) {
		dprintf("ext2: error occurred while reading inode %" PRId32 " (%d)\n", num, ret);
		kfree(inode);
		return ret;
	} else if(bytes != inode->size) {
		kfree(inode);
		return -ERR_FORMAT_INVAL;
	}

	dprintf("ext2: read inode %" PRId32 " from %" PRId64 " (group: %zu, block: %zu)\n",
		num, inode->offset, group, le32_to_cpu(mount->group_tbl[group].bg_inode_table));

	*inodep = inode;
	return 0;
}

/** Get the raw block number from an inode block number.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to get block for. Should be locked.
 * @param block		Block number within the inode to get.
 * @param nonblock	Whether to allow blocking.
 * @param nump		Where to store raw block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_block_get(ext2_inode_t *inode, uint32_t block, bool nonblock, uint32_t *nump) {
	uint32_t *i_block = NULL, *bi_block = NULL, num;
	int ret = 0;

	/* First check if it's a direct block. This is easy to handle, just
	 * need to get it straight out of the inode structure. */
	if(block < EXT2_NDIR_BLOCKS) {
		*nump = le32_to_cpu(inode->disk.i_block[block]);
		goto out;
	}

	block -= EXT2_NDIR_BLOCKS;
	i_block = kmalloc(inode->mount->blk_size, MM_SLEEP);

	/* Check whether the indirect block contains the block number we need.
	 * The indirect block contains as many 32-bit entries as will fit in
	 * one block of the filesystem. */
	if(block < (inode->mount->blk_size / sizeof(uint32_t))) {
		num = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
		if(num == 0) {
			*nump = 0;
			goto out;
		} else if((ret = ext2_block_read(inode->mount, i_block, num, nonblock)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		*nump = le32_to_cpu(i_block[block]);
		goto out;
	}

	block -= inode->mount->blk_size / sizeof(uint32_t);
	bi_block = kmalloc(inode->mount->blk_size, MM_SLEEP);

	/* Not in the indirect block, check the bi-indirect blocks. The
	 * bi-indirect block contains as many 32-bit entries as will fit in
	 * one block of the filesystem, with each entry pointing to an
	 * indirect block. */
	if(block < ((inode->mount->blk_size / sizeof(uint32_t)) * (inode->mount->blk_size / sizeof(uint32_t)))) {
		num = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK]);
		if(num == 0) {
			*nump = 0;
			goto out;
		} else if((ret = ext2_block_read(inode->mount, bi_block, num, nonblock)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		/* Get indirect block inside bi-indirect block. */
		num = le32_to_cpu(bi_block[block / (inode->mount->blk_size / sizeof(uint32_t))]);
		if(num == 0) {
			*nump = 0;
			goto out;
		} else if((ret = ext2_block_read(inode->mount, i_block, num, nonblock)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		*nump = le32_to_cpu(i_block[block % (inode->mount->blk_size / sizeof(uint32_t))]);
		goto out;
	}

	/* Triple indirect block. TODO. */
	dprintf("ext2: tri-indirect blocks not yet supported!\n");
	ret = -ERR_NOT_IMPLEMENTED;
out:
	if(bi_block) {
		kfree(bi_block);
	}
	if(i_block) {
		kfree(i_block);
	}

	return (ret < 0) ? ret : 0;
}

/** Allocate an inode block.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to allocate for. Should be write-locked.
 * @param block		Block number to allocate.
 * @param nonblock	Whether to allow blocking.
 * @param rawp		Where to store raw block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_block_alloc(ext2_inode_t *inode, uint32_t block, bool nonblock, uint32_t *rawp) {
	uint32_t *i_block = NULL, *bi_block = NULL, raw = 0, i_raw, bi_raw;
	int ret;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	/* Allocate a new raw block. */
	if((ret = ext2_block_alloc(inode->mount, nonblock, &raw)) != 0) {
		return ret;
	}

	/* First check if it's a direct block. This is easy to handle, just
	 * stick it straight into the inode structure. */
	if(block < EXT2_NDIR_BLOCKS) {
		/* This is braindead, i_blocks is the number of 512-byte
		 * blocks, not the number of <block size> blocks. Who the hell
		 * thought that up? */
		I_BLOCKS_INC(inode);
		inode->disk.i_block[block] = cpu_to_le32(raw);
		inode->dirty = true;
		goto out;
	}

	block -= EXT2_NDIR_BLOCKS;
	i_block = kmalloc(inode->mount->blk_size, MM_SLEEP);

	/* Check whether the block is in the indirect block. */
	if(block < (inode->mount->blk_size / sizeof(uint32_t))) {
		if((i_raw = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK])) == 0) {
			dprintf("ext2: allocating indirect block for %p(%" PRId32 ")\n", inode, inode->num);

			/* Allocate a new indirect block. */
			if((ret = ext2_block_alloc(inode->mount, nonblock, &i_raw)) != 0) {
				goto out;
			}

			memset(i_block, 0, inode->mount->blk_size);
			inode->disk.i_block[EXT2_IND_BLOCK] = cpu_to_le32(i_raw);
			I_BLOCKS_INC(inode);
			inode->dirty = true;
		} else {
			if((ret = ext2_block_read(inode->mount, i_block, i_raw, nonblock)) != 1) {
				ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
				goto out;
			}
		}

		i_block[block] = cpu_to_le32(raw);

		/* Write back the updated block. */
		if((ret = ext2_block_write(inode->mount, i_block, i_raw, nonblock)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		I_BLOCKS_INC(inode);
		inode->dirty = true;
		goto out;
	}

	block -= inode->mount->blk_size / sizeof(uint32_t);
	bi_block = kmalloc(inode->mount->blk_size, MM_SLEEP);

	/* Try the bi-indirect block. */
	if(block < ((inode->mount->blk_size / sizeof(uint32_t)) * (inode->mount->blk_size / sizeof(uint32_t)))) {
		if((bi_raw = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK])) == 0) {
			dprintf("ext2: allocating bi-indirect block for %p(%" PRId32 ")\n", inode, inode->num);

			/* Allocate a new bi-indirect block. */
			if((ret = ext2_block_alloc(inode->mount, nonblock, &bi_raw)) != 0) {
				goto out;
			}

			memset(bi_block, 0, inode->mount->blk_size);
			inode->disk.i_block[EXT2_DIND_BLOCK] = cpu_to_le32(bi_raw);
			I_BLOCKS_INC(inode);
			inode->dirty = true;
		} else {
			if((ret = ext2_block_read(inode->mount, bi_block, bi_raw, nonblock)) != 1) {
				ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
				goto out;
			}
		}

		if((i_raw = le32_to_cpu(bi_block[block / (inode->mount->blk_size / sizeof(uint32_t))])) == 0) {
			dprintf("ext2: allocating indirect block for %p(%" PRId32 ")\n", inode, inode->num);

			/* Allocate a new indirect block. */
			if((ret = ext2_block_alloc(inode->mount, nonblock, &i_raw)) != 0) {
				goto out;
			}

			memset(i_block, 0, inode->mount->blk_size);
			bi_block[block / (inode->mount->blk_size / sizeof(uint32_t))] = cpu_to_le32(i_raw);
			I_BLOCKS_INC(inode);
			inode->dirty = true;
		} else {
			if((ret = ext2_block_read(inode->mount, i_block, i_raw, nonblock)) != 1) {
				ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
				goto out;
			}
		}

		i_block[block] = cpu_to_le32(raw);

		/* Write back the updated block. */
		if((ret = ext2_block_write(inode->mount, i_block, i_raw, nonblock)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		I_BLOCKS_INC(inode);
		inode->dirty = true;
		goto out;
	}

	/* Triple indirect block. TODO. */
	dprintf("ext2: tri-indirect blocks not yet supported!\n");
	ret = -ERR_NOT_IMPLEMENTED;
out:
	if(bi_block != NULL) { kfree(bi_block); };
	if(i_block != NULL) { kfree(i_block); };
	if(raw) {
		if(ret < 0) {
			ext2_block_free(inode->mount, raw);
		} else {
			*rawp = raw;
		}
	}

	return (ret < 0) ? ret : 0;
}

/** Free an inode block.
 * @param inode		Inode to free from.
 * @param num		Pointer to block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_block_free(ext2_inode_t *inode, uint32_t *num) {
	int ret;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	if((ret = ext2_block_free(inode->mount, le32_to_cpu(*num))) != 0) {
		return ret;
	}

	I_BLOCKS_DEC(inode);
	inode->dirty = true;
	*num = 0;
	return 0;
}

/** Free an indirect block and all blocks it refers to.
 * @param inode		Inode to free from.
 * @param num		Pointer to block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_iblock_free(ext2_inode_t *inode, uint32_t *num) {
	uint32_t *block = kmalloc(inode->mount->blk_size, MM_SLEEP), i;
	int ret;

	/* Read in the block. */
	if((ret = ext2_block_read(inode->mount, block, le32_to_cpu(*num), false)) != 1) {
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Loop through each entry and free the blocks. */
	for(i = 0; i < (inode->mount->blk_size / sizeof(uint32_t)); i++) {
		if(block[i] == 0) {
			continue;
		} else if((ret = ext2_inode_block_free(inode, &block[i])) != 0) {
			kfree(block);
			return ret;
		}
	}

	kfree(block);

	/* Free the block itself. Don't need to write the block back because
	 * it's being freed. */
	return ext2_inode_block_free(inode, num);
}

/** Free a bi-indirect block.
 * @param inode		Inode to free from.
 * @param num		Pointer to block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_biblock_free(ext2_inode_t *inode, uint32_t *num) {
	uint32_t *block = kmalloc(inode->mount->blk_size, MM_SLEEP), i;
	int ret;

	/* Read in the block. */
	if((ret = ext2_block_read(inode->mount, block, le32_to_cpu(*num), false)) != 1) {
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Loop through each entry and free the blocks. */
	for(i = 0; i < (inode->mount->blk_size / sizeof(uint32_t)); i++) {
		if(block[i] == 0) {
			continue;
		} else if((ret = ext2_inode_iblock_free(inode, &block[i])) != 0) {
			kfree(block);
			return ret;
		}
	}

	kfree(block);

	/* Free the block itself. Don't need to write the block back because
	 * it's being freed. */
	return ext2_inode_block_free(inode, num);
}

/** Truncate an Ext2 inode.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to truncate. Should be locked.
 * @param size		New size of node.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_truncate(ext2_inode_t *inode, uint32_t size) {
	int i, ret;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	if(le32_to_cpu(inode->disk.i_size) <= size) {
		return 0;
	}

	/* TODO. I'm lazy. */
	if(size != 0) {
		return -ERR_NOT_IMPLEMENTED;
	}

	/* Don't support tri-indirect yet, check now so we don't discover
	 * one when we've already freed part of the file. */
	if(le32_to_cpu(inode->disk.i_block[EXT2_TIND_BLOCK]) != 0) {
		dprintf("ext2: tri-indirect blocks not yet supported!\n");
		return -ERR_NOT_IMPLEMENTED;
	}

	for(i = 0; i < EXT2_N_BLOCKS; i++) {
		if(le32_to_cpu(inode->disk.i_block[i]) == 0) {
			continue;
		} else if(i < EXT2_NDIR_BLOCKS) {
			if((ret = ext2_inode_block_free(inode, &inode->disk.i_block[i])) != 0) {
				return ret;
			}
		} else if(i == EXT2_IND_BLOCK) {
			if((ret = ext2_inode_iblock_free(inode, &inode->disk.i_block[i])) != 0) {
				return ret;
			}
		} else if(i == EXT2_DIND_BLOCK) {
			if((ret = ext2_inode_biblock_free(inode, &inode->disk.i_block[i])) != 0) {
				return ret;
			}
		}
	}

	inode->disk.i_size = cpu_to_le32(size);
	inode->dirty = true;
	return 0;
}

/** Allocate a new inode on an Ext2 filesystem.
 * @param mount		Mount to allocate on.
 * @param mode		Mode for inode.
 * @param inodep	Where to store pointer to new inode.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_alloc(ext2_mount_t *mount, uint16_t mode, ext2_inode_t **inodep) {
	uint32_t *block, num, in, count, i, j;
	ext2_group_desc_t *group;
	ext2_inode_t *inode;
	int ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	rwlock_write_lock(&mount->lock);

	if(le32_to_cpu(mount->sb.s_free_inodes_count) == 0) {
		rwlock_unlock(&mount->lock);
		return -ERR_NO_SPACE;
	}

	/* Iterate through all block groups to find one with free inodes. */
	for(num = 0; num < mount->blk_groups; num++) {
		group = &mount->group_tbl[num];
		if(le16_to_cpu(group->bg_free_inodes_count) == 0) {
			continue;
		}

		/* Work out how many blocks there are for the inode bitmap. */
		count = (mount->inodes_per_group / 8) / mount->blk_size;
		count = (count > 0) ? count : 1;

		/* Iterate through all inodes in the bitmap. */
		block = kmalloc(mount->blk_size, MM_SLEEP);
		for(i = 0; i < count; i++) {
			if((ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false)) != 1) {
				rwlock_unlock(&mount->lock);
				kfree(block);
				return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			}

			for(j = 0; j < ((mount->blk_size / sizeof(uint32_t)) * 32); j++) {
				if(block[j / 32] & (1 << (j % 32))) {
					continue;
				}

				goto found;
			}
		}

		kprintf(LOG_WARN, "ext2: inconsistency: group %" PRIu32 " has %" PRIu16 " inodes free, but none found\n",
			num, le16_to_cpu(group->bg_free_inodes_count));
		kfree(block);
		rwlock_unlock(&mount->lock);
		return -ERR_DEVICE_ERROR;
	}

	kprintf(LOG_WARN, "ext2: inconsistency: superblock has %" PRIu32 " inodes free, but none found\n",
		le32_to_cpu(mount->sb.s_free_inodes_count));
	rwlock_unlock(&mount->lock);
	return -ERR_DEVICE_ERROR;
found:
	/* Mark the inode as allocated and write back the bitmap block. */
	block[j / 32] |= (1 << (j % 32));
	if((ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	kfree(block);

	/* Update usage counts and write back the modified structures. */
	if((mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
		group->bg_used_dirs_count = cpu_to_le16(le16_to_cpu(group->bg_used_dirs_count) + 1);
	}
	group->bg_free_inodes_count = cpu_to_le16(le16_to_cpu(group->bg_free_inodes_count) - 1);
	mount->sb.s_free_inodes_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_inodes_count) - 1);
	ext2_mount_flush(mount);

	in = (num * mount->inodes_per_group) + (i * (mount->blk_size * 8)) + j + 1;

	/* Get the inode and set up information. */
	if((ret = ext2_inode_get_internal(mount, in, &inode)) != 0) {
		/* Ick. */
		rwlock_unlock(&mount->lock);
		ext2_inode_free(mount, in, mode);
		return ret;
	}

	inode->disk.i_uid = 0;
	inode->disk.i_gid = 0;
	inode->disk.i_mode = mode;
	inode->disk.i_size = 0;
	//inode->disk.i_atime = time;
	//inode->disk.i_ctime = time;
	//inode->disk.i_mtime = time;
	inode->disk.i_dtime = 0;
	inode->disk.i_blocks = 0;
	inode->disk.i_flags = 0;
	inode->disk.i_file_acl = 0;
	inode->disk.i_dir_acl = 0;
	memset(inode->disk.i_block, 0, sizeof(inode->disk.i_block));
	inode->dirty = true;

	dprintf("ext2: allocated inode %" PRIu32 " on %p (group: %" PRIu32 ")\n", in, mount, num);
	rwlock_unlock(&mount->lock);
	*inodep = inode;
	return 0;
}

/** Free an inode on an Ext2 filesystem.
 * @param mount		Mount to free on.
 * @param num		Block number to free.
 * @param mode		Mode of inode. This is required to determine whether
 *			the block group directory count needs to be decreased.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_free(ext2_mount_t *mount, uint32_t num, uint16_t mode) {
	uint32_t *block, gnum, i, off;
	ext2_group_desc_t *group;
	int ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	rwlock_write_lock(&mount->lock);

	/* Inode numbers are 1-based. */
	num -= 1;

	/* Work out the group containing the inode. */
	if((gnum = num / mount->inodes_per_group) >= mount->blk_groups) {
		rwlock_unlock(&mount->lock);
		return -ERR_PARAM_INVAL;
	}
	group = &mount->group_tbl[gnum];

	/* Get the block within the bitmap that contains the inode. */
	i = (num % mount->inodes_per_group) / 8 / mount->blk_size;
	block = kmalloc(mount->blk_size, MM_SLEEP);
	if((ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Mark the block as free and write back the bitmap block. */
	off = (num % mount->inodes_per_group) - (i * 8 * mount->blk_size);
	block[off / 32] &= ~(1 << (off % 32));
	if((ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	kfree(block);

	/* Update usage counts and write back the modified structures. */
	if((mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
		group->bg_used_dirs_count = cpu_to_le16(le16_to_cpu(group->bg_used_dirs_count) - 1);
	}
	group->bg_free_inodes_count = cpu_to_le16(le16_to_cpu(group->bg_free_inodes_count) + 1);
	mount->sb.s_free_inodes_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_inodes_count) + 1);
	ext2_mount_flush(mount);

	dprintf("ext2: freed inode %u on %p (group: %" PRIu32 ", i: %" PRIu32 ")\n",
		num + 1, mount, gnum, i);
	rwlock_unlock(&mount->lock);
	return 0;
}

/** Get an inode from an Ext2 filesystem.
 * @param mount		Mount to read from.
 * @param num		Inode number to read.
 * @param inodep	Where to store pointer to inode structure.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **inodep) {
	int ret;

	rwlock_read_lock(&mount->lock);
	ret = ext2_inode_get_internal(mount, num, inodep);
	rwlock_unlock(&mount->lock);
	return ret;
}

/** Flush changes to an Ext2 inode structure.
 * @note		This is protected by the VFS node/mount locks.
 * @param inode		Inode to flush.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_flush(ext2_inode_t *inode) {
	size_t bytes;
	int ret;

	if(!inode->dirty) {
		return 0;
	}

	if((ret = device_write(inode->mount->device, &inode->disk, inode->size, inode->offset, &bytes)) != 0) {
		kprintf(LOG_WARN, "ext2: error occurred while writing inode %" PRId32 " (%d)\n", inode->num, ret);
		return ret;
	} else if(bytes != inode->size) {
		kprintf(LOG_WARN, "ext2: could not write all data for inode %" PRId32 "\n", inode->num);
		return -ERR_DEVICE_ERROR;
	}

	inode->dirty = false;
	return 0;
}

/** Free an in-memory inode structure.
 * @param inode		Inode to free. Should not be dirty. */
void ext2_inode_release(ext2_inode_t *inode) {
	assert(!inode->dirty);

	if(le16_to_cpu(inode->disk.i_links_count) == 0) {
		assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

		dprintf("ext2: inode %p(%" PRId32 ") has no links remaining, freeing...\n", inode, inode->num);

		/* Update deletion time and truncate the inode. */
		//inode->disk.i_dtime = time_get_current();
		inode->disk.i_dtime = cpu_to_le32(1252603142);
		ext2_inode_truncate(inode, 0);
		inode->dirty = true;
		ext2_inode_flush(inode);

		ext2_inode_free(inode->mount, inode->num, le16_to_cpu(inode->disk.i_mode));
	}
	kfree(inode);
}

/** Read blocks from an Ext2 inode.
 * @param inode		Inode to read from (read-/write-locked).
 * @param buf		Buffer to read into.
 * @param block		Starting block number.
 * @param count		Number of blocks to read.
 * @param nonblock	Whether to allow blocking.
 * @return		Number of blocks read on success, negative error code
 *			on failure. */
int ext2_inode_read(ext2_inode_t *inode, void *buf, uint32_t block, size_t count, bool nonblock) {
	uint32_t total, i, raw = 0;
	int ret;

	total = ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->blk_size) / inode->mount->blk_size;
	if(block >= total || !count) {
		return 0;
	} else if((block + count) > total) {
		count = total - block;
	}

	for(i = 0; i < count; i++, buf += inode->mount->blk_size) {
		if((ret = ext2_inode_block_get(inode, block + i, nonblock, &raw)) != 0) {
			dprintf("ext2: failed to lookup raw block for inode %p(%" PRId32 ") (%d)\n",
			        inode, inode->num, ret);
			return ret;
		}

		/* If the block number is 0, then it's a sparse block. */
		if(raw == 0) {
			memset(buf, 0, inode->mount->blk_size);
		} else {
			if((ret = ext2_block_read(inode->mount, buf, raw, nonblock)) != 1) {
				return (ret < 0) ? ret : (int)i;
			}
		}
	}

	return i;
}

/** Write blocks to an Ext2 inode.
 * @param inode		Inode to write to (write-locked).
 * @param buf		Buffer to write from.
 * @param block		Starting block number.
 * @param count		Number of blocks to write.
 * @param nonblock	Whether to allow blocking.
 * @return		Number of blocks written on success, negative error
 *			code on failure. */
int ext2_inode_write(ext2_inode_t *inode, const void *buf, uint32_t block, size_t count, bool nonblock) {
	uint32_t total, i, raw = 0;
	int ret;

	total = ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->blk_size) / inode->mount->blk_size;
	if(block >= total || !count) {
		return 0;
	} else if((block + count) > total) {
		count = total - block;
	}

	for(i = 0; i < count; i++, buf += inode->mount->blk_size) {
		if((ret = ext2_inode_block_get(inode, block + i, nonblock, &raw)) != 0) {
			dprintf("ext2: failed to lookup raw block for inode %p(%" PRId32 ") (%d)\n",
			        inode, inode->num, ret);
			return ret;
		}

		/* If the block number is 0, then allocate a new block. */
		if(raw == 0) {
			if((ret = ext2_inode_block_alloc(inode, block + i, nonblock, &raw)) != 0) {
				dprintf("ext2: failed to allocate raw block for inode %p(%" PRId32 ") (%d)\n",
				        inode, inode->num, ret);
				return ret;
			}
		}

		if((ret = ext2_block_write(inode->mount, buf, raw, nonblock)) != 1) {
			return (ret < 0) ? ret : (int)i;
		}
	}

	return i;
}

/** Resize an Ext2 inode.
 * @param inode		Node to resize (write-locked).
 * @param size		New size of file.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_resize(ext2_inode_t *inode, file_size_t size) {
	int ret = 0;

	if(size >= ((uint64_t)1<<32)) {
		return -ERR_NOT_SUPPORTED;
	}

	if(size > le32_to_cpu(inode->disk.i_size)) {
		assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));
		inode->disk.i_size = cpu_to_le32((uint32_t)size);
		inode->dirty = true;
	} else if(size < le32_to_cpu(inode->disk.i_size)) {
		ret = ext2_inode_truncate(inode, size);
	}

	return ret;
}
