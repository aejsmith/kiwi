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

#include "ext2_priv.h"

/** Get the raw block number from an inode block number.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to get block for. Should be locked.
 * @param block		Block number within the inode to get.
 * @param nump		Where to store raw block number.
 * @return		0 on success, negative error code on failure. */
static int ext2_inode_block_get(ext2_inode_t *inode, uint32_t block, uint32_t *nump) {
	uint32_t *i_block = NULL, *bi_block = NULL, num;
	int ret = 0;

	/* First check if it's a direct block. This is easy to handle, just
	 * need to get it straight out of the inode structure. */
	if(block < EXT2_NDIR_BLOCKS) {
		*nump = le32_to_cpu(inode->disk.i_block[block]);
		goto out;
	}

	block -= EXT2_NDIR_BLOCKS;

	/* We need a buffer to store blocks in. */
	i_block = kmalloc(inode->mount->blk_size, MM_SLEEP);

	/* Check whether the indirect block contains the block number we need.
	 * The indirect block contains as many 32-bit entries as will fit in
	 * one block of the filesystem. */
	if(block < (inode->mount->blk_size / sizeof(uint32_t))) {
		num = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
		if(num == 0) {
			*nump = 0;
			goto out;
		} else if((ret = ext2_block_read(inode->mount, i_block, num)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		*nump = le32_to_cpu(i_block[block]);
		goto out;
	}

	block -= inode->mount->blk_size / 4;

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
		} else if((ret = ext2_block_read(inode->mount, bi_block, num)) != 1) {
			ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
			goto out;
		}

		/* Get indirect block inside bi-indirect block. */
		num = le32_to_cpu(bi_block[block / (inode->mount->blk_size / sizeof(uint32_t))]);
		if(num == 0) {
			*nump = 0;
			goto out;
		} else if((ret = ext2_block_read(inode->mount, i_block, num)) != 1) {
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

/** Get an inode from an Ext2 filesystem.
 * @param mount		Mount to read from.
 * @param num		Inode number to read.
 * @param inodep	Where to store pointer to inode structure.
 * @return		0 on success, negative error code on failure. */
int ext2_inode_get(ext2_mount_t *mount, identifier_t num, ext2_inode_t **inodep) {
	ext2_inode_t *inode = NULL;
	size_t group, bytes;
	offset_t offset;
	int ret;

	rwlock_read_lock(&mount->lock, 0);

	/* Get the group descriptor table containing the inode. */
	if((group = (num - 1) / mount->inodes_per_group) >= mount->blk_groups) {
		dprintf("ext2: group number %zu is invalid on mount %p\n", group, mount);
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	/* Get the offset of the inode in the group's inode table. */
	offset = ((num - 1) % mount->inodes_per_group) * mount->in_size;

	/* Create a structure to store details of the inode in memory. */
	inode = kmalloc(sizeof(ext2_inode_t), MM_SLEEP);
	rwlock_init(&inode->lock, "ext2_inode_lock");
	inode->num = num;
	inode->size = (mount->in_size <= sizeof(ext2_disk_inode_t)) ? mount->in_size : sizeof(ext2_disk_inode_t);
	inode->offset = ((offset_t)le32_to_cpu(mount->group_tbl[group].bg_inode_table) * mount->blk_size) + offset;
	inode->mount = mount;

	/* Read it in. */
	if((ret = device_read(mount->device, &inode->disk, inode->size, inode->offset, &bytes)) != 0) {
		dprintf("ext2: error occurred while reading inode %" PRId32 " (%d)\n", num, ret);
		goto fail;
	} else if(bytes != inode->size) {
		ret = -ERR_FORMAT_INVAL;
		goto fail;
	}

	dprintf("ext2: read inode %" PRId32 " from %" PRId64 " (group: %zu, block: %zu)\n",
		num, inode->offset, group, le32_to_cpu(mount->group_tbl[group].bg_inode_table));

	rwlock_unlock(&mount->lock);
	*inodep = inode;
	return 0;
fail:
	if(inode) {
		kfree(inode);
	}
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
		return 0;
	} else if(bytes != inode->size) {
		kprintf(LOG_WARN, "ext2: could not write all data for inode %" PRId32 "\n", inode->num);
		return -ERR_DEVICE_ERROR;
	}

	return 0;
}

/** Free an in-memory inode structure.
 * @param inode		Inode to free. Should not be dirty. */
void ext2_inode_release(ext2_inode_t *inode) {
	assert(!inode->dirty);
	kfree(inode);
}

/** Read blocks from an Ext2 inode.
 * @param inode		Inode to read from.
 * @param buf		Buffer to read into.
 * @param block		Starting block number.
 * @param count		Number of blocks to read.
 * @return		Number of blocks read on success, negative error code
 *			on failure. */
int ext2_inode_read(ext2_inode_t *inode, void *buf, uint32_t block, size_t count) {
	uint32_t total, i, raw = 0;
	int ret;

	rwlock_read_lock(&inode->lock, 0);

	total = ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->blk_size) / inode->mount->blk_size;
	if(block >= total || !count) {
		rwlock_unlock(&inode->lock);
		return 0;
	} else if((block + count) > total) {
		count = total - block;
	}

	for(i = 0; i < count; i++, buf += inode->mount->blk_size) {
		if((ret = ext2_inode_block_get(inode, block + i, &raw)) != 0) {
			dprintf("ext2: failed to lookup raw block for inode %p(%" PRId32 ") (%d)\n",
			        inode, inode->num, ret);
			rwlock_unlock(&inode->lock);
			return ret;
		}

		/* If the block number is 0, then it's a sparse block. */
		if(raw == 0) {
			memset(buf, 0, inode->mount->blk_size);
		} else {
			if((ret = ext2_block_read(inode->mount, buf, raw)) != 1) {
				rwlock_unlock(&inode->lock);
				return (ret < 0) ? ret : (int)i;
			}
		}
	}

	rwlock_unlock(&inode->lock);
	return i;
}
