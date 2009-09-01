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

#include <errors.h>

#include "ext2_priv.h"

/** Read in a block from an EXT2 filesystem.
 * @param mount		Mount to read from.
 * @param buf		Buffer to read into.
 * @param block		Block number to read.
 * @return		1 if block read, 0 if block doesn't exist, negative
 *			error code on failure. */
int ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t block) {
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
 * @return		1 if block written, 0 if block doesn't exist, negative
 *			error code on failure. */
int ext2_block_write(ext2_mount_t *mount, const void *buf, uint32_t block) {
	size_t bytes;
	int ret;

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
