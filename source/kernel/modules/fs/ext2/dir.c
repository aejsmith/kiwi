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

/** Cache entries in an Ext2 directory.
 * @param node		VFS node to cache.
 * @return		0 on success, negative error code on failure. */
int ext2_dir_cache(vfs_node_t *node) {
	ext2_inode_t *inode = node->data;
	ext2_dirent_t *dirent;
	uint32_t current = 0;
	uint8_t *buf = NULL;
	char *name = NULL;
	int count, ret;

	assert(node->type == VFS_NODE_DIR);

	/* Work out how many blocks to read. */
	count = ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->blk_size) / inode->mount->blk_size;

	/* Allocate buffers to read the data into. Don't use MM_SLEEP for the
	 * first as it could be rather large. */
	if(!(buf = kmalloc(count * inode->mount->blk_size, 0))) {
		return -ERR_NO_MEMORY;
	}
	name = kmalloc(EXT2_NAME_MAX + 1, MM_SLEEP);

	/* Read in all the directory entries required. */
	if((ret = ext2_inode_read(inode, buf, 0, count)) != count) {
		dprintf("ext2: could not read all directory data for inode %p(%" PRId32 ") (%d)\n",
			inode, inode->num, ret);
		ret = (ret < 0) ? ret : -ERR_DEVICE_ERROR;
		goto out;
	}

	while(current < le32_to_cpu(inode->disk.i_size)) {
		dirent = (ext2_dirent_t *)(buf + current);
		current += le16_to_cpu(dirent->rec_len);

		if(dirent->file_type != EXT2_FT_UNKNOWN && dirent->name_len != 0) {
			strncpy(name, dirent->name, dirent->name_len);
			name[dirent->name_len] = 0;
			vfs_dir_entry_add(node, le32_to_cpu(dirent->inode), name);
		} else if(!le16_to_cpu(dirent->rec_len)) {
			dprintf("ext2: directory entry length was 0 on inode %p(%" PRId32 ")\n",
			        inode, inode->num);
			ret = -ERR_DEVICE_ERROR;
			goto out;
		}
	}

	ret = 0;
out:
	if(buf) {
		kfree(buf);
	}
	if(name) {
		kfree(name);
	}
	return ret;
}
