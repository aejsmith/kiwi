/*
 * Copyright (C) 2008-2010 Alex Smith
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
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
#include <lib/utility.h>

#include <mm/malloc.h>

#include <assert.h>
#include <endian.h>
#include <status.h>
#include <time.h>

#include "ext2_priv.h"

/** Get the number of entries per block. */
#define ENTRIES_PER_BLOCK(i)	((i)->mount->block_size / sizeof(uint32_t))

/** Get the raw block number from an inode block number.
 * @todo		Triple indirect blocks.
 * @param map		File map to get for.
 * @param block		Block number within the inode to get.
 * @param rawp		Where to store raw block number.
 * @return		Status code describing result of the operation. */
static status_t ext2_map_lookup(file_map_t *map, uint64_t block, uint64_t *rawp) {
	uint32_t *i_block = NULL, *bi_block = NULL;
	ext2_inode_t *inode = map->data;
	status_t ret = STATUS_SUCCESS;
	uint32_t num;

	dprintf("ext2: looking up block %" PRIu64 " within inode %p(%" PRIu32 ")\n",
	        block, inode, inode->num);

	/* First check if it's a direct block. This is easy to handle, just
	 * need to get it straight out of the inode structure. */
	if(block < EXT2_NDIR_BLOCKS) {
		*rawp = (uint64_t)le32_to_cpu(inode->disk.i_block[block]);
		goto out;
	}

	block -= EXT2_NDIR_BLOCKS;
	i_block = kmalloc(inode->mount->block_size, MM_KERNEL);

	/* Check whether the indirect block contains the block number we need.
	 * The indirect block contains as many 32-bit entries as will fit in
	 * one block of the filesystem. */
	if(block < ENTRIES_PER_BLOCK(inode)) {
		num = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
		if(num == 0) {
			*rawp = 0;
			goto out;
		}

		ret = ext2_block_read(inode->mount, i_block, num, false);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		*rawp = (uint64_t)le32_to_cpu(i_block[block]);
		goto out;
	}

	block -= ENTRIES_PER_BLOCK(inode);
	bi_block = kmalloc(inode->mount->block_size, MM_KERNEL);

	/* Not in the indirect block, check the bi-indirect blocks. The
	 * bi-indirect block contains as many 32-bit entries as will fit in
	 * one block of the filesystem, with each entry pointing to an
	 * indirect block. */
	if(block < (ENTRIES_PER_BLOCK(inode) * ENTRIES_PER_BLOCK(inode))) {
		num = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK]);
		if(num == 0) {
			*rawp = 0;
			goto out;
		}

		ret = ext2_block_read(inode->mount, bi_block, num, false);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		/* Get indirect block inside bi-indirect block. */
		num = le32_to_cpu(bi_block[block / ENTRIES_PER_BLOCK(inode)]);
		if(num == 0) {
			*rawp = 0;
			goto out;
		}

		ret = ext2_block_read(inode->mount, i_block, num, false);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		*rawp = (uint64_t)le32_to_cpu(i_block[block % ENTRIES_PER_BLOCK(inode)]);
		goto out;
	}

	/* Triple indirect block. TODO. */
	kprintf(LOG_WARN, "ext2: tri-indirect blocks not yet supported!\n");
	ret = STATUS_NOT_IMPLEMENTED;
out:
	if(bi_block) {
		kfree(bi_block);
	}
	if(i_block) {
		kfree(i_block);
	}
	if(ret == STATUS_SUCCESS) {
		dprintf("ext2: looked up to %" PRIu64 "\n", *rawp);
	}
	return ret;
}

/** Read a raw Ext2 block.
 * @param map		Map the read is for.
 * @param buf		Buffer to read into.
 * @param num		Raw block number.
 * @param nonblock	Whether the operation is required to not block.
 * @return		Status code describing result of the operation. */
static status_t ext2_map_read_block(file_map_t *map, void *buf, uint64_t num, bool nonblock) {
	ext2_inode_t *inode = map->data;

	dprintf("ext2: reading raw block %" PRIu64 " for inode %p(%" PRIu32 ")\n",
	        num, inode, inode->num);

	if(num == 0) {
		/* Sparse block, fill with zeros. */
		memset(buf, 0, inode->mount->block_size);
		return STATUS_SUCCESS;
	} else {
		return ext2_block_read(inode->mount, buf, num, nonblock);
	}
}

/** Write a raw Ext2 block.
 * @param map		Map the write is for.
 * @param buf		Buffer containing data to write.
 * @param num		Raw block number.
 * @param nonblock	Whether the operation is required to not block.
 * @return		Status code describing result of the operation. */
static status_t ext2_map_write_block(file_map_t *map, const void *buf, uint64_t num, bool nonblock) {
	ext2_inode_t *inode = map->data;

	dprintf("ext2: writing raw block %" PRIu64 " for inode %p(%" PRIu32 ")\n",
	        num, inode, inode->num);

	if(num != 0) {
		return ext2_block_write(inode->mount, buf, num, nonblock);
	} else {
		return STATUS_SUCCESS;
	}
}

/** Ext2 file map operations. */
static file_map_ops_t ext2_file_map_ops = {
	.lookup = ext2_map_lookup,
	.read_block = ext2_map_read_block,
	.write_block = ext2_map_write_block,
};

/** Allocate an inode block.
 * @todo		Triple indirect blocks.
 * @param inode		Inode to allocate for. Should be locked.
 * @param block		Block number to allocate.
 * @param nonblock	Whether to allow blocking.
 * @return		Status code describing result of the operation. */
static status_t ext2_inode_block_alloc(ext2_inode_t *inode, uint32_t block, bool nonblock) {
	uint32_t *i_block = NULL, *bi_block = NULL, raw = 0, i_raw, bi_raw;
	status_t ret;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	/* Allocate a new raw block. */
	ret = ext2_block_alloc(inode->mount, nonblock, &raw);
	if(ret != STATUS_SUCCESS) {
		return ret;
	}
	ret = ext2_block_zero(inode->mount, raw);
	if(ret != STATUS_SUCCESS) {
		goto out;
	}

	dprintf("ext2: mapping block %" PRIu32 " within inode %p(%" PRIu32 ") to %" PRIu32 "\n",
	        block, inode, inode->num, raw);

	/* First check if it's a direct block. This is easy to handle, just
	 * stick it straight into the inode structure. */
	if(block < EXT2_NDIR_BLOCKS) {
		/* This is braindead, i_blocks is the number of 512-byte
		 * blocks, not the number of <block size> blocks. Who the hell
		 * thought that up? */
		I_BLOCKS_INC(inode);
		inode->disk.i_block[block] = cpu_to_le32(raw);
		ext2_inode_flush(inode);
		goto out;
	}

	block -= EXT2_NDIR_BLOCKS;
	i_block = kmalloc(inode->mount->block_size, MM_KERNEL);

	/* Check whether the block is in the indirect block. */
	if(block < ENTRIES_PER_BLOCK(inode)) {
		i_raw = le32_to_cpu(inode->disk.i_block[EXT2_IND_BLOCK]);
		if(i_raw == 0) {
			dprintf("ext2: allocating indirect block for %p(%" PRIu32 ")\n", inode, inode->num);

			/* Allocate a new indirect block. */
			ret = ext2_block_alloc(inode->mount, nonblock, &i_raw);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}

			inode->disk.i_block[EXT2_IND_BLOCK] = cpu_to_le32(i_raw);

			I_BLOCKS_INC(inode);
			ext2_inode_flush(inode);
			memset(i_block, 0, inode->mount->block_size);
		} else {
			ret = ext2_block_read(inode->mount, i_block, i_raw, nonblock);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}
		}

		i_block[block] = cpu_to_le32(raw);

		/* Write back the updated block. */
		ret = ext2_block_write(inode->mount, i_block, i_raw, nonblock);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		I_BLOCKS_INC(inode);
		ext2_inode_flush(inode);
		goto out;
	}

	block -= ENTRIES_PER_BLOCK(inode);
	bi_block = kmalloc(inode->mount->block_size, MM_KERNEL);

	/* Try the bi-indirect block. */
	if(block < (ENTRIES_PER_BLOCK(inode) * ENTRIES_PER_BLOCK(inode))) {
		bi_raw = le32_to_cpu(inode->disk.i_block[EXT2_DIND_BLOCK]);
		if(bi_raw == 0) {
			dprintf("ext2: allocating bi-indirect block for %p(%" PRIu32 ")\n", inode, inode->num);

			/* Allocate a new bi-indirect block. */
			ret = ext2_block_alloc(inode->mount, nonblock, &bi_raw);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}

			inode->disk.i_block[EXT2_DIND_BLOCK] = cpu_to_le32(bi_raw);

			I_BLOCKS_INC(inode);
			ext2_inode_flush(inode);
			memset(bi_block, 0, inode->mount->block_size);
		} else {
			ret = ext2_block_read(inode->mount, bi_block, bi_raw, nonblock);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}
		}

		i_raw = le32_to_cpu(bi_block[block / ENTRIES_PER_BLOCK(inode)]);
		if(i_raw == 0) {
			dprintf("ext2: allocating indirect block for %p(%" PRIu32 ")\n", inode, inode->num);

			/* Allocate a new indirect block. */
			ret = ext2_block_alloc(inode->mount, nonblock, &i_raw);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}

			bi_block[block / ENTRIES_PER_BLOCK(inode)] = cpu_to_le32(i_raw);

			/* Write back the updated block. */
			ret = ext2_block_write(inode->mount, bi_block, bi_raw, nonblock);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}

			I_BLOCKS_INC(inode);
			ext2_inode_flush(inode);
			memset(i_block, 0, inode->mount->block_size);
		} else {
			ret = ext2_block_read(inode->mount, i_block, i_raw, nonblock);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}
		}

		i_block[block % ENTRIES_PER_BLOCK(inode)] = cpu_to_le32(raw);

		/* Write back the updated block. */
		ret = ext2_block_write(inode->mount, i_block, i_raw, nonblock);
		if(ret != STATUS_SUCCESS) {
			goto out;
		}

		I_BLOCKS_INC(inode);
		ext2_inode_flush(inode);
		goto out;
	}

	/* Triple indirect block. TODO. */
	kprintf(LOG_WARN, "ext2: tri-indirect blocks not yet supported!\n");
	ret = STATUS_NOT_IMPLEMENTED;
out:
	if(ret == STATUS_SUCCESS) {
		if(bi_block) {
			file_map_invalidate(inode->map, block + EXT2_NDIR_BLOCKS + ENTRIES_PER_BLOCK(inode), 1);
		} else if(i_block) {
			file_map_invalidate(inode->map, block + EXT2_NDIR_BLOCKS, 1);
		} else {
			file_map_invalidate(inode->map, block, 1);
		}
	} else {
		ext2_block_free(inode->mount, raw);
	}
	if(bi_block) { kfree(bi_block); };
	if(i_block) { kfree(i_block); };
	return ret;
}

/** Free an inode block.
 * @param inode		Inode to free from.
 * @param num		Pointer to block number.
 * @return		Status code describing result of the operation. */
static status_t ext2_inode_block_free(ext2_inode_t *inode, uint32_t *num) {
	status_t ret;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	ret = ext2_block_free(inode->mount, le32_to_cpu(*num));
	if(ret != STATUS_SUCCESS) {
		return ret;
	}

	I_BLOCKS_DEC(inode);
	ext2_inode_flush(inode);
	*num = 0;
	return STATUS_SUCCESS;
}

/** Free an indirect block and all blocks it refers to.
 * @param inode		Inode to free from.
 * @param num		Pointer to block number.
 * @return		Status code describing result of the operation. */
static status_t ext2_inode_iblock_free(ext2_inode_t *inode, uint32_t *num) {
	uint32_t *block = kmalloc(inode->mount->block_size, MM_KERNEL), i;
	status_t ret;

	/* Read in the block. */
	ret = ext2_block_read(inode->mount, block, le32_to_cpu(*num), false);
	if(ret != STATUS_SUCCESS) {
		kfree(block);
		return ret;
	}

	/* Loop through each entry and free the blocks. */
	for(i = 0; i < ENTRIES_PER_BLOCK(inode); i++) {
		if(block[i] == 0) {
			continue;
		}

		ret = ext2_inode_block_free(inode, &block[i]);
		if(ret != STATUS_SUCCESS) {
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
 * @return		Status code describing result of the operation. */
static status_t ext2_inode_biblock_free(ext2_inode_t *inode, uint32_t *num) {
	uint32_t *block = kmalloc(inode->mount->block_size, MM_KERNEL), i;
	status_t ret;

	/* Read in the block. */
	ret = ext2_block_read(inode->mount, block, le32_to_cpu(*num), false);
	if(ret != STATUS_SUCCESS) {
		kfree(block);
		return ret;
	}

	/* Loop through each entry and free the blocks. */
	for(i = 0; i < ENTRIES_PER_BLOCK(inode); i++) {
		if(block[i] == 0) {
			continue;
		}

		ret = ext2_inode_iblock_free(inode, &block[i]);
		if(ret != STATUS_SUCCESS) {
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
 * @return		Status code describing result of the operation. */
static status_t ext2_inode_truncate(ext2_inode_t *inode, offset_t size) {
	status_t ret;
	size_t count;
	int i;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	if(inode->size <= size) {
		return STATUS_SUCCESS;
	}

	/* TODO. I'm lazy. */
	if(size > 0) {
		kprintf(LOG_WARN, "ext2: truncate not yet support for size > 0\n");
		return STATUS_NOT_IMPLEMENTED;
	}

	/* Don't support tri-indirect yet, check now so we don't discover
	 * one when we've already freed part of the file. */
	if(le32_to_cpu(inode->disk.i_block[EXT2_TIND_BLOCK]) != 0) {
		kprintf(LOG_WARN, "ext2: tri-indirect blocks not yet supported!\n");
		return STATUS_NOT_IMPLEMENTED;
	}

	count = ROUND_UP(inode->size, inode->mount->block_size) / inode->mount->block_size;
	file_map_invalidate(inode->map, 0, count);
	vm_cache_resize(inode->cache, size);
	inode->size = size;
	inode->disk.i_mtime = cpu_to_le32(USECS2SECS(unix_time()));
	ext2_inode_flush(inode);

	for(i = 0; i < EXT2_N_BLOCKS; i++) {
		if(le32_to_cpu(inode->disk.i_block[i]) == 0) {
			continue;
		} else if(i < EXT2_NDIR_BLOCKS) {
			ret = ext2_inode_block_free(inode, &inode->disk.i_block[i]);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		} else if(i == EXT2_IND_BLOCK) {
			ret = ext2_inode_iblock_free(inode, &inode->disk.i_block[i]);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		} else if(i == EXT2_DIND_BLOCK) {
			ret = ext2_inode_biblock_free(inode, &inode->disk.i_block[i]);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
		}
	}

	return STATUS_SUCCESS;
}

/** Allocate a new inode on an Ext2 filesystem.
 * @param mount		Mount to allocate on.
 * @param mode		File type mode for the new node. Permission bits will
 *			be ignored.
 * @param security	Security attributes for the node.
 * @param inodep	Where to store pointer to new inode.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_alloc(ext2_mount_t *mount, uint16_t mode, const object_security_t *security,
                          ext2_inode_t **inodep) {
	uint32_t *block, num, in, count, i, j;
	ext2_group_desc_t *group;
	ext2_inode_t *inode;
	uint32_t time;
	status_t ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	mutex_lock(&mount->lock);

	if(le32_to_cpu(mount->sb.s_free_inodes_count) == 0) {
		mutex_unlock(&mount->lock);
		return STATUS_FS_FULL;
	}

	/* Iterate through all block groups to find one with free inodes. */
	for(num = 0; num < mount->block_groups; num++) {
		group = &mount->group_tbl[num];
		if(le16_to_cpu(group->bg_free_inodes_count) == 0) {
			continue;
		}

		/* Work out how many blocks there are for the inode bitmap. */
		count = (mount->inodes_per_group / 8) / mount->block_size;
		count = (count > 0) ? count : 1;

		/* Iterate through all inodes in the bitmap. */
		block = kmalloc(mount->block_size, MM_KERNEL);
		for(i = 0; i < count; i++) {
			ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false);
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

		kprintf(LOG_WARN, "ext2: inconsistency: group %" PRIu32 " has %" PRIu16 " inodes free, but none found\n",
			num, le16_to_cpu(group->bg_free_inodes_count));
		kfree(block);
		mutex_unlock(&mount->lock);
		return STATUS_CORRUPT_FS;
	}

	kprintf(LOG_WARN, "ext2: inconsistency: superblock has %" PRIu32 " inodes free, but none found\n",
		le32_to_cpu(mount->sb.s_free_inodes_count));
	mutex_unlock(&mount->lock);
	return STATUS_CORRUPT_FS;
found:
	/* Mark the inode as allocated and write back the bitmap block. */
	block[j / 32] |= (1 << (j % 32));
	ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
	}

	kfree(block);

	/* Update usage counts and write back the modified structures. */
	if((mode & EXT2_S_IFMT) == EXT2_S_IFDIR) {
		group->bg_used_dirs_count = cpu_to_le16(le16_to_cpu(group->bg_used_dirs_count) + 1);
	}
	group->bg_free_inodes_count = cpu_to_le16(le16_to_cpu(group->bg_free_inodes_count) - 1);
	mount->sb.s_free_inodes_count = cpu_to_le32(le32_to_cpu(mount->sb.s_free_inodes_count) - 1);
	ext2_mount_flush(mount);

	in = (num * mount->inodes_per_group) + (i * (mount->block_size * 8)) + j + 1;

	/* Get the inode and set up information. */
	ret = ext2_inode_get(mount, in, &inode);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		ext2_inode_free(mount, in, mode);
		return ret;
	}

	inode->size = 0;

	time = USECS2SECS(unix_time());
	inode->disk.i_mode = cpu_to_le16(mode & EXT2_S_IFMT);
	inode->disk.i_size = 0;
	inode->disk.i_atime = cpu_to_le32(time);
	inode->disk.i_ctime = cpu_to_le32(time);
	inode->disk.i_mtime = cpu_to_le32(time);
	inode->disk.i_dtime = 0;
	inode->disk.i_blocks = 0;
	inode->disk.i_flags = 0;
	inode->disk.i_file_acl = 0;
	inode->disk.i_dir_acl = 0;
	memset(inode->disk.i_block, 0, sizeof(inode->disk.i_block));
	ext2_inode_flush(inode);

	/* Set security attributes on the node. */
	ret = ext2_inode_set_security(inode, security);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		ext2_inode_release(inode);
		return ret;
	}

	dprintf("ext2: allocated inode %" PRIu32 " on %p (group: %" PRIu32 ")\n", in, mount, num);
	mutex_unlock(&mount->lock);
	*inodep = inode;
	return STATUS_SUCCESS;
}

/** Free an inode on an Ext2 filesystem.
 * @param mount		Mount to free on.
 * @param num		Block number to free.
 * @param mode		Mode of inode. This is required to determine whether
 *			the block group directory count needs to be decreased.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_free(ext2_mount_t *mount, uint32_t num, uint16_t mode) {
	uint32_t *block, gnum, i, off;
	ext2_group_desc_t *group;
	status_t ret;

	assert(!(mount->parent->flags & FS_MOUNT_RDONLY));

	mutex_lock(&mount->lock);

	/* Inode numbers are 1-based. */
	num -= 1;

	/* Work out the group containing the inode. */
	gnum = num / mount->inodes_per_group;
	if(gnum >= mount->block_groups) {
		mutex_unlock(&mount->lock);
		return STATUS_CORRUPT_FS;
	}
	group = &mount->group_tbl[gnum];

	/* Get the block within the bitmap that contains the inode. */
	i = (num % mount->inodes_per_group) / 8 / mount->block_size;
	block = kmalloc(mount->block_size, MM_KERNEL);
	ret = ext2_block_read(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
	}

	/* Mark the block as free and write back the bitmap block. */
	off = (num % mount->inodes_per_group) - (i * 8 * mount->block_size);
	block[off / 32] &= ~(1 << (off % 32));
	ret = ext2_block_write(mount, block, le32_to_cpu(group->bg_inode_bitmap) + i, false);
	if(ret != STATUS_SUCCESS) {
		mutex_unlock(&mount->lock);
		kfree(block);
		return ret;
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
	mutex_unlock(&mount->lock);
	return STATUS_SUCCESS;
}

/** Get an inode from an Ext2 filesystem.
 * @note		Node creation/lookup are protected by the mount lock,
 *			meaning this function does not need to lock.
 * @param mount		Mount to read from.
 * @param num		Inode number to read.
 * @param inodep	Where to store pointer to inode structure.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **inodep) {
	ext2_inode_t *inode = NULL;
	size_t group, bytes;
	offset_t offset;
	status_t ret;

	/* Get the group descriptor table containing the inode. */
	group = (num - 1) / mount->inodes_per_group;
	if(group >= mount->block_groups) {
		dprintf("ext2: group number %zu is invalid on mount %p\n", group, mount);
		return STATUS_CORRUPT_FS;
	}

	/* Get the offset of the inode in the group's inode table. */
	offset = ((num - 1) % mount->inodes_per_group) * mount->inode_size;

	/* Create a structure to store details of the inode in memory. */
	inode = kmalloc(sizeof(ext2_inode_t), MM_KERNEL);
	mutex_init(&inode->lock, "ext2_inode_lock", MUTEX_RECURSIVE);
	inode->mount = mount;
	inode->num = num;
	inode->disk_size = MIN(mount->inode_size, sizeof(ext2_disk_inode_t));
	inode->disk_offset = ((offset_t)le32_to_cpu(mount->group_tbl[group].bg_inode_table) * mount->block_size) + offset;

	/* Read it in. */
	ret = device_read(mount->device, &inode->disk, inode->disk_size, inode->disk_offset, &bytes);
	if(ret != STATUS_SUCCESS) {
		dprintf("ext2: error occurred while reading inode %" PRIu32 " (%d)\n", num, ret);
		kfree(inode);
		return ret;
	} else if(bytes != inode->disk_size) {
		kfree(inode);
		return STATUS_CORRUPT_FS;
	}

	/* Work out the size of the node data. Regular files can be larger than
	 * 4GB - the high 32-bits of the file size are stored in i_dir_acl. */
	inode->size = le32_to_cpu(inode->disk.i_size);
	if(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFREG) {
		inode->size |= ((uint64_t)le32_to_cpu(inode->disk.i_dir_acl)) << 32;
	}

	/* Create the various caches. */
	inode->map = file_map_create(mount->block_size, &ext2_file_map_ops, inode);
	inode->cache = vm_cache_create(inode->size, &file_map_vm_cache_ops, inode->map);
	inode->entries = entry_cache_create(&ext2_entry_cache_ops, inode);

	dprintf("ext2: read inode %" PRIu32 " from %" PRIu64 " (group: %zu, block: %zu)\n",
		num, inode->disk_offset, group,
		le32_to_cpu(mount->group_tbl[group].bg_inode_table));
	*inodep = inode;
	return STATUS_SUCCESS;
}

/** Flush changes to an Ext2 inode structure.
 * @note		Does not flush the data cache.
 * @param inode		Inode to flush.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_flush(ext2_inode_t *inode) {
	status_t ret;
	size_t bytes;

	/* Copy the data size back to the inode structure. */
	inode->disk.i_size = cpu_to_le32(inode->size);
	if(le16_to_cpu(inode->disk.i_mode) & EXT2_S_IFREG) {
		if(inode->size >= 0x80000000) {
			/* Set the large file feature flag if it is not already set. */
			if(!EXT2_HAS_RO_COMPAT_FEATURE(&inode->mount->sb, EXT2_FEATURE_RO_COMPAT_LARGE_FILE)) {
				EXT2_SET_RO_COMPAT_FEATURE(&inode->mount->sb, EXT2_FEATURE_RO_COMPAT_LARGE_FILE);
				ext2_mount_flush(inode->mount);
			}
			inode->disk.i_dir_acl = cpu_to_le32(inode->size >> 32);
		}
	}

	ret = device_write(inode->mount->device, &inode->disk, inode->disk_size, inode->disk_offset, &bytes);
	if(ret != STATUS_SUCCESS) {
		kprintf(LOG_WARN, "ext2: error occurred while writing inode %" PRIu32 " (%d)\n", inode->num, ret);
		return ret;
	} else if(bytes != inode->disk_size) {
		kprintf(LOG_WARN, "ext2: could not write all data for inode %" PRIu32 "\n", inode->num);
		return STATUS_CORRUPT_FS;
	}

	return STATUS_SUCCESS;
}

/** Free an in-memory inode structure.
 * @param inode		Inode to free. */
void ext2_inode_release(ext2_inode_t *inode) {
	if(le16_to_cpu(inode->disk.i_links_count) == 0) {
		assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

		dprintf("ext2: inode %p(%" PRIu32 ") has no links remaining, freeing...\n", inode, inode->num);

		/* Update deletion time and truncate the inode. */
		inode->disk.i_dtime = cpu_to_le32(USECS2SECS(unix_time()));
		ext2_inode_truncate(inode, 0);
		ext2_inode_flush(inode);

		ext2_inode_free(inode->mount, inode->num, le16_to_cpu(inode->disk.i_mode));
	}

	entry_cache_destroy(inode->entries);
	vm_cache_destroy(inode->cache, false);
	file_map_destroy(inode->map);
	kfree(inode);
}

/** Read from an Ext2 inode.
 * @param inode		Inode to read from.
 * @param buf		Buffer to read into.
 * @param count		Number of bytes to read.
 * @param offset	Offset into inode to read from.
 * @param nonblock	Whether the operation is required to not block.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_read(ext2_inode_t *inode, void *buf, size_t count, offset_t offset,
                         bool nonblock, size_t *bytesp) {
	return vm_cache_read(inode->cache, buf, count, offset, nonblock, bytesp);
}

/** Read to an Ext2 inode.
 * @param inode		Inode to write to.
 * @param buf		Buffer containing data to write.
 * @param count		Number of bytes to write.
 * @param offset	Offset into inode to write to.
 * @param nonblock	Whether the operation is required to not block.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_write(ext2_inode_t *inode, const void *buf, size_t count, offset_t offset,
                          bool nonblock, size_t *bytesp) {
	uint32_t start, blocks, i;
	uint64_t raw;
	status_t ret;

	mutex_lock(&inode->lock);

	/* Attempt to resize the node if necessary. */
	if((offset + count) > inode->size) {    
		inode->size = offset + count;
		vm_cache_resize(inode->cache, inode->size);
		ext2_inode_flush(inode);
	}

	/* Now we need to reserve blocks on the filesystem. */
	start = offset / inode->mount->block_size;
	blocks = (ROUND_UP(offset + count, inode->mount->block_size) / inode->mount->block_size) - start;
	for(i = 0; i < blocks; i++) {
		ret = file_map_lookup(inode->map, start + i, &raw);
		if(ret != STATUS_SUCCESS) {
			dprintf("ext2: failed to lookup raw block for inode %p(%" PRIu32 ") (%d)\n",
			        inode, inode->num, ret);
			return ret;
		}

		/* If the block number is 0, then allocate a new block. The
		 * call to ext2_inode_block_alloc() invalidates the file map
		 * entries. */
		if(raw == 0) {
			ret = ext2_inode_block_alloc(inode, start + i, nonblock);
			if(ret != STATUS_SUCCESS) {
				dprintf("ext2: failed to allocate raw block for inode %p(%" PRIu32 ") (%d)\n",
				        inode, inode->num, ret);
				return ret;
			}
		}
	}

	mutex_unlock(&inode->lock);

	ret = vm_cache_write(inode->cache, buf, count, offset, nonblock, bytesp);
	if(*bytesp) {
		inode->disk.i_mtime = cpu_to_le32(USECS2SECS(unix_time()));
	}

	return ret;
}

/** Resize an Ext2 inode.
 * @param inode		Node to resize.
 * @param size		New size of file.
 * @return		Status code describing result of the operation. */
status_t ext2_inode_resize(ext2_inode_t *inode, offset_t size) {
	status_t ret = STATUS_SUCCESS;

	assert(!(inode->mount->parent->flags & FS_MOUNT_RDONLY));

	mutex_lock(&inode->lock);

	if(size > inode->size) {
		inode->size = size;
		vm_cache_resize(inode->cache, size);
		ext2_inode_flush(inode);
	} else if(size < inode->size) {
		ret = ext2_inode_truncate(inode, size);
	}

	mutex_unlock(&inode->lock);
	return ret;
}
