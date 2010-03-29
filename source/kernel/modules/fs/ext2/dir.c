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
int ext2_dir_cache(fs_node_t *node) {
	ext2_inode_t *inode = node->data;
	char *buf = NULL, *name = NULL;
	ext2_dirent_t *dirent;
	uint32_t current = 0;
	int count, ret;

	rwlock_read_lock(&inode->lock);

	assert(node->type == FS_NODE_DIR);

	/* Work out how many blocks to read. */
	count = ROUND_UP(le32_to_cpu(inode->disk.i_size), inode->mount->block_size) / inode->mount->block_size;

	/* Allocate buffers to read the data into. Don't use MM_SLEEP for the
	 * first as it could be rather large. */
	if(!(buf = kmalloc(count * inode->mount->block_size, 0))) {
		rwlock_unlock(&inode->lock);
		return -ERR_NO_MEMORY;
	}
	name = kmalloc(EXT2_NAME_MAX + 1, MM_SLEEP);

	/* Read in all the directory entries required. */
	if((ret = ext2_inode_read(inode, buf, 0, count, false)) != count) {
		dprintf("ext2: could not read all directory data for inode %p(%" PRIu64 ") (%d)\n",
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
			fs_dir_insert(node, name, le32_to_cpu(dirent->inode));
		} else if(!le16_to_cpu(dirent->rec_len)) {
			dprintf("ext2: directory entry length was 0 on inode %p(%" PRIu64 ")\n",
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
	rwlock_unlock(&inode->lock);
	return ret;
}

/** Insert an entry into a directory.
 * @param dir		Directory to insert into (write-locked).
 * @param inode		Inode to insert (write-locked).
 * @param name		Name to give the entry.
 * @return		0 on success, negative error code on failure. */
int ext2_dir_insert(ext2_inode_t *dir, ext2_inode_t *inode, const char *name) {
	size_t name_len, rec_len, exist_len;
	uint32_t current = 0, raw;
	ext2_dirent_t *dirent;
	int count, ret, i;
	char *buf;

	assert((le16_to_cpu(dir->disk.i_mode) & EXT2_S_IFMT) == EXT2_S_IFDIR);
	assert(!(dir->mount->parent->flags & FS_MOUNT_RDONLY));

	/* Quote: 'It should be noted that some implementation will pad
	 * directory entries to have better performances on the host
	 * processor'. */
	name_len = strlen(name);
	rec_len = ROUND_UP((sizeof(ext2_dirent_t) - EXT2_NAME_MAX) + name_len, 4);

	/* Work out how many blocks to read. */
	count = ROUND_UP(le32_to_cpu(dir->disk.i_size), dir->mount->block_size) / dir->mount->block_size;

	/* Allocate a buffer to read the data into. Don't use MM_SLEEP as it
	 * could be rather large. */
	if(!(buf = kmalloc(count * dir->mount->block_size, 0))) {
		return -ERR_NO_MEMORY;
	}

	/* Read in all the directory entries required. */
	if((ret = ext2_inode_read(dir, buf, 0, count, false)) != count) {
		dprintf("ext2: could not read all directory data for inode %p(%" PRIu64 ") (%d)\n",
			dir, dir->num, ret);
		kfree(buf);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Search for a free directory entry in the existing blocks. */
	while(current < le32_to_cpu(dir->disk.i_size)) {
		dirent = (ext2_dirent_t *)(buf + current);
		current += le16_to_cpu(dirent->rec_len);
		exist_len = ROUND_UP((sizeof(ext2_dirent_t) - EXT2_NAME_MAX) + dirent->name_len, 4);

		if(le16_to_cpu(dirent->rec_len) < rec_len) {
			continue;
		} else if(le32_to_cpu(dirent->inode) != 0) {
			if(le16_to_cpu(dirent->rec_len) < (exist_len + rec_len)) {
				continue;
			}

			/* Split the entry in two. */
			rec_len = le16_to_cpu(dirent->rec_len) - exist_len;
			dirent->rec_len = cpu_to_le16(exist_len);
			dirent = (ext2_dirent_t *)((ptr_t)dirent + exist_len);
			dirent->rec_len = cpu_to_le16(rec_len);
		}

		dirent->inode = cpu_to_le32(inode->num);
		dirent->name_len = name_len;
		dirent->file_type = ext2_type_to_dirent(le16_to_cpu(inode->disk.i_mode));

		memcpy(dirent->name, name, name_len);

		/* Write back the entry. */
		if((ret = ext2_inode_write(dir, buf, 0, count, false)) != count) {
			dprintf("ext2: could not write all directory data for inode %p(%" PRIu64 ") (%d)\n",
				dir, dir->num, ret);
			kfree(buf);
			return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
		}

		/* Update inode link count. */
		inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) + 1);
		ext2_inode_flush(inode);
		kfree(buf);
		return 0;
	}

	/* Couldn't find a spare entry. Allocate a block for a new one. */
	for(i = 0; i < EXT2_NDIR_BLOCKS; i++) {
		if(le32_to_cpu(dir->disk.i_block[i]) != 0) {
			continue;
		} else if((ret = ext2_block_alloc(dir->mount, false, &raw)) != 0) {
			kfree(buf);
			return ret;
		}
		dir->disk.i_block[i] = cpu_to_le32(raw);
		ext2_inode_flush(dir);

		dirent = kcalloc(1, dir->mount->block_size, MM_SLEEP);
		dirent->inode = cpu_to_le32(inode->num);
		dirent->rec_len = cpu_to_le16(dir->mount->block_size);
		dirent->name_len = name_len;
		dirent->file_type = ext2_type_to_dirent(le16_to_cpu(inode->disk.i_mode));

		memcpy(dirent->name, name, name_len);

		if((ret = ext2_block_write(dir->mount, dirent, raw, false)) != 1) {
			dprintf("ext2: could not write new block for inode %p(%" PRIu64 ") (%d)\n",
			        dir, dir->num, ret);
			kfree(dirent);
			kfree(buf);
			return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
		}

		dir->disk.i_size = cpu_to_le32(le32_to_cpu(dir->disk.i_size) + dir->mount->block_size);
		I_BLOCKS_INC(dir);

		inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) + 1);
		ext2_inode_flush(inode);
		kfree(dirent);
		kfree(buf);
		return 0;
	}

	kfree(buf);
	return -ERR_NO_SPACE;
}

/** Remove an entry from a directory.
 * @param dir		Directory to remove from (write-locked).
 * @param inode		Inode corresponding to the entry (write-locked).
 * @param name		Name of the entry to remove.
 * @return		0 on success, negative error code on failure. */
int ext2_dir_remove(ext2_inode_t *dir, ext2_inode_t *inode, const char *name) {
	ext2_dirent_t *dirent, *last = NULL;
	uint32_t current = 0;
	size_t name_len;
	int count, ret;
	char *buf;

	assert((le16_to_cpu(dir->disk.i_mode) & EXT2_S_IFMT) == EXT2_S_IFDIR);
	assert(!(dir->mount->parent->flags & FS_MOUNT_RDONLY));

	name_len = strlen(name);

	/* Work out how many blocks to read. */
	count = ROUND_UP(le32_to_cpu(dir->disk.i_size), dir->mount->block_size) / dir->mount->block_size;

	/* Allocate a buffer to read the data into. Don't use MM_SLEEP as it
	 * could be rather large. */
	if(!(buf = kmalloc(count * dir->mount->block_size, 0))) {
		return -ERR_NO_MEMORY;
	}

	/* Read in all the directory entries required. */
	if((ret = ext2_inode_read(dir, buf, 0, count, false)) != count) {
		dprintf("ext2: could not read all directory data for inode %p(%" PRIu64 ") (%d)\n",
			dir, dir->num, ret);
		kfree(buf);
		return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
	}

	/* Search for the entry. */
	while(current < le32_to_cpu(dir->disk.i_size)) {
		dirent = (ext2_dirent_t *)(buf + current);
		current += le16_to_cpu(dirent->rec_len);

		if(le32_to_cpu(dirent->inode) == 0) {
			last = dirent;
			continue;
		} else if(dirent->name_len != name_len || strncmp(dirent->name, name, name_len) != 0) {
			last = dirent;
			continue;
		}

		assert(le32_to_cpu(dirent->inode) == inode->num);
		dirent->inode = 0;
		if(last != NULL) {
			last->rec_len = cpu_to_le16(le16_to_cpu(last->rec_len) + le16_to_cpu(dirent->rec_len));
		}

		/* Write back the entry. */
		if((ret = ext2_inode_write(dir, buf, 0, count, false)) != count) {
			dprintf("ext2: could not write all directory data for inode %p(%" PRIu64 ") (%d)\n",
				dir, dir->num, ret);
			kfree(buf);
			return (ret < 0) ? ret : -ERR_DEVICE_ERROR;
		}

		inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) - 1);
		ext2_inode_flush(inode);
		kfree(buf);
		return 0;
	}

	dprintf("ext2: could not find directory entry '%s' being removed from %p(%u)\n",
	        name, dir, dir->num);
	kfree(buf);
	return 0;
}
