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

#include <mm/malloc.h>

#include <assert.h>
#include <endian.h>
#include <errors.h>

#include "ext2_priv.h"

/** Allocate a new block on an Ext2 filesystem.
 * @param mount		Mount to allocate on.
 * @param nonblock	Whether to allow blocking.
 * @param blockp	Where to store number of new block.
 * @return		0 on success, negative error code on failure. */
int ext2_block_alloc(ext2_mount_t *mount, bool nonblock, uint32_t *blockp) {
	uint32_t *block, num, count, i, j;
	ext2_group_desc_t *group;
	int ret;

	assert(!(mount->parent->flags & VFS_MOUNT_RDONLY));

	rwlock_write_lock(&mount->lock, 0);

	if(le32_to_cpu(mount->sb.s_free_blocks_count) == 0) {
		rwlock_unlock(&mount->lock);
		return -ERR_NO_SPACE;
	}

	/* Iterate through all block groups to find one with free blocks. */
	for(num = 0; num < mount->blk_groups; num++) {
		group = &mount->group_tbl[num];
		if(le16_to_cpu(group->bg_free_blocks_count) == 0) {
			continue;
		}

		/* Work out how many blocks there are for the block bitmap. */
		count = (mount->blocks_per_group / 8) / mount->blk_size;

		/* Iterate through all blocks in the bitmap. */
		block = kmalloc(mount->blk_size, MM_SLEEP);
		for(i = 0; i < count; i++) {
			if((ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, nonblock)) != 1) {
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

		kprintf(LOG_WARN, "ext2: inconsistency: group %" PRIu32 " has %" PRIu16 " blocks free, but none found\n",
			num, le16_to_cpu(group->bg_free_blocks_count));
		kfree(block);
		rwlock_unlock(&mount->lock);
		return -ERR_DEVICE_ERROR;
	}

	kprintf(LOG_WARN, "ext2: inconsistency: superblock has %" PRIu32 " blocks free, but none found\n",
		le32_to_cpu(mount->sb.s_free_blocks_count));
	rwlock_unlock(&mount->lock);
	return -ERR_DEVICE_ERROR;
found:
	/* Mark the block as allocated and write back the bitmap block. */
	block[j / 32] |= (1 << (j % 32));
	if((ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, nonblock)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	kfree(block);

	/* Update usage counts and write back the modified structures. */
	group->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(group->bg_free_blocks_count) - 1);
	mount->sb.s_free_blocks_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_blocks_count) - 1);
	ext2_mount_flush(mount);

	*blockp = (num * mount->blocks_per_group) + (i * (mount->blk_size * 8)) + 
	          j + le32_to_cpu(mount->sb.s_first_data_block);
	dprintf("ext2: allocated block %" PRIu32 " on %p (group: %" PRIu32 ")\n", *blockp, mount, num);
	rwlock_unlock(&mount->lock);
	return 0;
}

/** Free a block on an Ext2 filesystem.
 * @param mount		Mount to free on.
 * @param num		Block number to free.
 * @return		0 on success, negative error code on failure. */
int ext2_block_free(ext2_mount_t *mount, uint32_t num) {
	uint32_t *block, gnum, i, off;
	ext2_group_desc_t *group;
	int ret;

	assert(!(mount->parent->flags & VFS_MOUNT_RDONLY));

	rwlock_write_lock(&mount->lock, 0);

	num -= le32_to_cpu(mount->sb.s_first_data_block);

	/* Work out the group containing the block. */
	if((gnum = num / mount->blocks_per_group) >= mount->blk_groups) {
		rwlock_unlock(&mount->lock);
		return -ERR_PARAM_INVAL;
	}
	group = &mount->group_tbl[gnum];

	/* Get the block within the bitmap that contains the block. */
	i = (num % mount->blocks_per_group) / 8 / mount->blk_size;
	block = kmalloc(mount->blk_size, MM_SLEEP);
	if((ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, false)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Mark the block as free and write back the bitmap block. */
	off = (num % mount->blocks_per_group) - (i * 8 * mount->blk_size);
	block[off / 32] &= ~(1 << (off % 32));
	if((ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, false)) != 1) {
		rwlock_unlock(&mount->lock);
		kfree(block);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	kfree(block);

	/* Update usage counts and write back the modified structures. */
	group->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(group->bg_free_blocks_count) + 1);
	mount->sb.s_free_blocks_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_blocks_count) + 1);
	ext2_mount_flush(mount);

	dprintf("ext2: freed block %u on %p (group: %" PRIu32 ", i: %" PRIu32 ")\n",
		num + le32_to_cpu(mount->sb.s_first_data_block), mount, gnum, i);
	rwlock_unlock(&mount->lock);
	return 0;
}

/** Read in a block from an EXT2 filesystem.
 * @param mount		Mount to read from.
 * @param buf		Buffer to read into.
 * @param block		Block number to read.
 * @param nonblock	Whether to allow blocking (TODO).
 * @return		1 if block read, 0 if block doesn't exist, negative
 *			error code on failure. */
int ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t block, bool nonblock) {
	size_t bytes;
	int ret;

	if(block > mount->blocks_count) {
		dprintf("ext2: attempted to read invalid block number %" PRIu32 " on mount %p\n", block, mount);
		return 0;
	}

	if((ret = device_read(mount->device, buf, mount->blk_size, block * mount->blk_size, &bytes)) != 0) {
		dprintf("ext2: failed to read block %" PRIu32 " (%d)\n", block, ret);
		return ret;
	} else if(bytes != mount->blk_size) {
		return -ERR_DEVICE_ERROR;
	}

	return 1;
}

/** Write a block to an EXT2 filesystem.
 * @param mount		Mount to write to.
 * @param buf		Buffer to write from
 * @param block		Block number to write.
 * @param nonblock	Whether to allow blocking (TODO).
 * @return		1 if block written, 0 if block doesn't exist, negative
 *			error code on failure. */
int ext2_block_write(ext2_mount_t *mount, const void *buf, uint32_t block, bool nonblock) {
	size_t bytes;
	int ret;

	assert(!(mount->parent->flags & VFS_MOUNT_RDONLY));

	if(block > mount->blocks_count) {
		dprintf("ext2: attempted to write invalid block number %" PRIu32 " on mount %p\n", block, mount);
		return 0;
	}

	if((ret = device_write(mount->device, buf, mount->blk_size, block * mount->blk_size, &bytes)) != 0) {
		dprintf("ext2: failed to write block %" PRIu32 " (%d)\n", block, ret);
		return ret;
	} else if(bytes != mount->blk_size) {
		return -ERR_DEVICE_ERROR;
	}

	return 1;
}
