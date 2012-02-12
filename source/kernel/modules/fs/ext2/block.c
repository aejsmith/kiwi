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
#include <endian.h>
#include <status.h>

#include "ext2_priv.h"

/** Zero a block.
 * @param mount		Mount to zero on.
 * @param block		Block number to zero.
 * @return		Status code describing result of the operation. */
status_t ext2_block_zero(ext2_mount_t *mount, uint32_t block) {
	status_t ret;
	char *data;

	data = kmalloc(mount->block_size, MM_WAIT | MM_ZERO);
	ret = ext2_block_write(mount, data, block, false);
	kfree(data);
	return ret;
}

/** Allocate a new block on an Ext2 filesystem.
 * @param mount		Mount to allocate on.
 * @param nonblock	Whether the operation is required to not block.
 * @param blockp	Where to store number of new block.
 * @return		Status code describing result of the operation. */
status_t ext2_block_alloc(ext2_mount_t *mount, bool nonblock, uint32_t *blockp) {
	ext2_group_desc_t *group;
	size_t num, count, i, j;
	uint32_t *block;
	status_t ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	mutex_lock(&mount->lock);

	if(le32_to_cpu(mount->sb.s_free_blocks_count) == 0) {
		mutex_unlock(&mount->lock);
		return STATUS_FS_FULL;
	}

	/* Iterate through all block groups to find one with free blocks. */
	for(num = 0; num < mount->block_groups; num++) {
		group = &mount->group_tbl[num];
		if(le16_to_cpu(group->bg_free_blocks_count) == 0) {
			continue;
		}

		/* Work out how many blocks there are for the block bitmap. */
		count = (mount->blocks_per_group / 8) / mount->block_size;

		/* Iterate through all blocks in the bitmap. */
		block = kmalloc(mount->block_size, MM_WAIT);
		for(i = 0; i < count; i++) {
			ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, nonblock);
			if(ret != STATUS_SUCCESS) {
				mutex_unlock(&mount->lock);
				kfree(block);
				return ret;
			}

			for(j = 0; j < ((mount->block_size / sizeof(uint32_t)) * 32); j++) {
				if(block[j / 32] & (1 << (j % 32))) {
					continue;
				}
				goto found;
			}
		}

		kprintf(LOG_WARN, "ext2: inconsistency: group %" PRIu32 " has %" PRIu16 " blocks free, but none found\n",
			num, le16_to_cpu(group->bg_free_blocks_count));
		kfree(block);
		mutex_unlock(&mount->lock);
		return STATUS_CORRUPT_FS;
	}

	kprintf(LOG_WARN, "ext2: inconsistency: superblock has %" PRIu32 " blocks free, but none found\n",
		le32_to_cpu(mount->sb.s_free_blocks_count));
	mutex_unlock(&mount->lock);
	return STATUS_CORRUPT_FS;
found:
	/* Mark the block as allocated and write back the bitmap block. */
	block[j / 32] |= (1 << (j % 32));
	ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, nonblock);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
	}
	kfree(block);

	/* Update usage counts and write back the modified structures. */
	group->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(group->bg_free_blocks_count) - 1);
	mount->sb.s_free_blocks_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_blocks_count) - 1);
	ext2_mount_flush(mount);

	*blockp = (num * mount->blocks_per_group) + (i * (mount->block_size * 8)) + 
	          j + le32_to_cpu(mount->sb.s_first_data_block);
	dprintf("ext2: allocated block %" PRIu32 " on %p (group: %" PRIu32 ")\n", *blockp, mount, num);
	mutex_unlock(&mount->lock);
	return STATUS_SUCCESS;
}

/** Free a block on an Ext2 filesystem.
 * @param mount		Mount to free on.
 * @param num		Block number to free.
 * @return		Status code describing result of the operation. */
status_t ext2_block_free(ext2_mount_t *mount, uint32_t num) {
	ext2_group_desc_t *group;
	size_t gnum, i, off;
	uint32_t *block;
	status_t ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	mutex_lock(&mount->lock);

	num -= le32_to_cpu(mount->sb.s_first_data_block);

	/* Work out the group containing the block. */
	gnum = num / mount->blocks_per_group;
	if(gnum >= mount->block_groups) {
		mutex_unlock(&mount->lock);
		return STATUS_CORRUPT_FS;
	}
	group = &mount->group_tbl[gnum];

	/* Get the block within the bitmap that contains the block. */
	i = (num % mount->blocks_per_group) / 8 / mount->block_size;
	block = kmalloc(mount->block_size, MM_WAIT);
	ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, false);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
	}

	/* Mark the block as free and write back the bitmap block. */
	off = (num % mount->blocks_per_group) - (i * 8 * mount->block_size);
	block[off / 32] &= ~(1 << (off % 32));
	ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_block_bitmap) + i, false);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
	}
	kfree(block);

	/* Update usage counts and write back the modified structures. */
	group->bg_free_blocks_count = cpu_to_le16(le16_to_cpu(group->bg_free_blocks_count) + 1);
	mount->sb.s_free_blocks_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_blocks_count) + 1);
	ext2_mount_flush(mount);

	dprintf("ext2: freed block %u on %p (group: %" PRIu32 ", i: %" PRIu32 ")\n",
		num + le32_to_cpu(mount->sb.s_first_data_block), mount, gnum, i);
	mutex_unlock(&mount->lock);
	return STATUS_SUCCESS;
}

/** Read in a block from an EXT2 filesystem.
 * @param mount		Mount to read from.
 * @param buf		Buffer to read into.
 * @param block		Block number to read.
 * @param nonblock	Whether the operation is required to not block (TODO).
 * @return		Status code describing result of the operation. */
status_t ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t block, bool nonblock) {
	status_t ret;
	size_t bytes;

	if(block > mount->block_count) {
		dprintf("ext2: attempted to read invalid block number %" PRIu32 " on mount %p\n",
		        block, mount);
		return STATUS_CORRUPT_FS;
	}

	ret = device_read(mount->device, buf, mount->block_size, block * mount->block_size, &bytes);
	if(ret != STATUS_SUCCESS) {
		dprintf("ext2: failed to read block %" PRIu32 " (%d)\n", block, ret);
		return ret;
	} else if(bytes != mount->block_size) {
		return STATUS_CORRUPT_FS;
	}

	return STATUS_SUCCESS;
}

/** Write a block to an EXT2 filesystem.
 * @param mount		Mount to write to.
 * @param buf		Buffer to write from
 * @param block		Block number to write.
 * @param nonblock	Whether the operation is required to not block (TODO).
 * @return		Status code describing result of the operation. */
status_t ext2_block_write(ext2_mount_t *mount, const void *buf, uint32_t block, bool nonblock) {
	status_t ret;
	size_t bytes;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	if(block > mount->block_count) {
		dprintf("ext2: attempted to write invalid block number %" PRIu32 " on mount %p\n",
		        block, mount);
		return STATUS_CORRUPT_FS;
	}

	ret = device_write(mount->device, buf, mount->block_size, block * mount->block_size, &bytes);
	if(ret != STATUS_SUCCESS) {
		dprintf("ext2: failed to write block %" PRIu32 " (%d)\n", block, ret);
		return ret;
	} else if(bytes != mount->block_size) {
		return STATUS_CORRUPT_FS;
	}

	return STATUS_SUCCESS;
}
