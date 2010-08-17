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
#include <status.h>

#include "ext2_priv.h"

/** Read a directory entry.
 * @param dir		Directory to read from.
 * @param buf		Buffer to read into.
 * @param offset	Offset to read from.
 * @param name		If not NULL, buffer to read name into (EXT2_NAME_MAX + 1).
 * @return		Status code describing result of the operation. */
static status_t ext2_dirent_read(ext2_inode_t *dir, ext2_dirent_t *buf, offset_t offset,
                                 char *name) {
	status_t ret;
	size_t bytes;

	ret = ext2_inode_read(dir, buf, sizeof(ext2_dirent_t), offset, false, &bytes);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(bytes != sizeof(ext2_dirent_t)) {
		return STATUS_CORRUPT_FS;
	}

	if(name) {
		offset += sizeof(ext2_dirent_t);
		ret = ext2_inode_read(dir, name, buf->name_len, offset, false, &bytes);
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(bytes != buf->name_len) {
			return STATUS_CORRUPT_FS;
		}
		name[buf->name_len] = 0;
	}

	return STATUS_SUCCESS;
}

/** Write a directory entry.
 * @param dir		Directory to write to.
 * @param buf		Buffer containing entry header.
 * @param offset	Offset to write to.
 * @param name		If not NULL, buffer containing new name to write. If
 *			NULL, the name will not be changed.
 * @return		Status code describing result of the operation. */
static status_t ext2_dirent_write(ext2_inode_t *dir, const ext2_dirent_t *buf, offset_t offset,
                                  const char *name) {
	status_t ret;
	size_t bytes;

	ret = ext2_inode_write(dir, buf, sizeof(ext2_dirent_t), offset, false, &bytes);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(bytes != sizeof(ext2_dirent_t)) {
		return STATUS_CORRUPT_FS;
	}

	if(name) {
		offset += sizeof(ext2_dirent_t);
		ret = ext2_inode_write(dir, name, buf->name_len, offset, false, &bytes);
		if(ret != STATUS_SUCCESS) {
			return ret;
		} else if(bytes != buf->name_len) {
			return STATUS_CORRUPT_FS;
		}
	}

	return STATUS_SUCCESS;
}

/** Iterate through entries in an Ext2 directory.
 * @param dir		Node to iterate through.
 * @param index		Index of entry to begin at.
 * @param cb		Callback function.
 * @param data		Argument to pass to callback.
 * @return		Status code describing result of the operation. */
status_t ext2_dir_iterate(ext2_inode_t *dir, offset_t index, ext2_dir_iterate_cb_t cb, void *data) {
	offset_t offset = 0, i = 0;
	ext2_dirent_t entry;
	bool found = false;
	status_t ret;
	char *name;

	mutex_lock(&dir->lock);

	name = kmalloc(EXT2_NAME_MAX + 1, MM_SLEEP);
	while(offset < dir->size) {
		if(i++ >= index) {
			ret = ext2_dirent_read(dir, &entry, offset, name);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}

			if(entry.file_type != EXT2_FT_UNKNOWN && entry.name_len != 0) {
				found = true;
				if(!cb(dir, &entry, name, offset, data)) {
					break;
				}
			}
		} else {
			ret = ext2_dirent_read(dir, &entry, offset, NULL);
			if(ret != STATUS_SUCCESS) {
				goto out;
			}
		}

		assert(entry.rec_len);
		offset += le16_to_cpu(entry.rec_len);
	}

	ret = STATUS_SUCCESS;
out:
	kfree(name);
	mutex_unlock(&dir->lock);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else {
		return (found) ? STATUS_SUCCESS : STATUS_NOT_FOUND;
	}
}

/** Insert an entry into a directory.
 * @note		Necessary locking should be done by caller.
 * @param dir		Directory to insert into.
 * @param name		Name to give the entry.
 * @param inode		Inode to insert.
 * @return		Status code describing result of the operation. */
status_t ext2_dir_insert(ext2_inode_t *dir, const char *name, ext2_inode_t *inode) {
	size_t name_len, rec_len, exist_len;
	offset_t offset = 0;
	ext2_dirent_t entry;
	uint32_t raw;
	status_t ret;
	size_t i;

	assert((le16_to_cpu(dir->disk.i_mode) & EXT2_S_IFMT) == EXT2_S_IFDIR);
	assert(!(dir->mount->parent->flags & FS_MOUNT_RDONLY));

	/* Quote: 'It should be noted that some implementations will pad
	 * directory entries to have better performances on the host
	 * processor'. */
	name_len = strlen(name);
	if(name_len > EXT2_NAME_MAX) {
		return STATUS_NOT_SUPPORTED;
	}
	rec_len = ROUND_UP(sizeof(ext2_dirent_t) + name_len, 4);

	dprintf("ext2: inserting entry '%s' to %" PRIu32 " into %" PRIu32 " (name_len: %zu, rec_len: %zu)\n",
	        name, inode->num, dir->num, name_len, rec_len);

	while(offset < dir->size) {
		ret = ext2_dirent_read(dir, &entry, offset, NULL);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		dprintf("ext2: existing entry at offset %" PRIu64 " (rec_len: %u, name_len: %u, inode: %" PRIu32 ")\n",
		        offset, le16_to_cpu(entry.rec_len), entry.name_len, le32_to_cpu(entry.inode));

		assert(entry.rec_len);

		if(le16_to_cpu(entry.rec_len) < rec_len) {
			offset += le16_to_cpu(entry.rec_len);
			continue;
		} else if(le32_to_cpu(entry.inode) != 0) {
			exist_len = ROUND_UP(sizeof(ext2_dirent_t) + entry.name_len, 4);
			if(le16_to_cpu(entry.rec_len) < (exist_len + rec_len)) {
				offset += le16_to_cpu(entry.rec_len);
				continue;
			}

			/* Split the entry in two. */
			rec_len = le16_to_cpu(entry.rec_len) - exist_len;
			entry.rec_len = cpu_to_le16(exist_len);
			ret = ext2_dirent_write(dir, &entry, offset, NULL);
			if(ret != STATUS_SUCCESS) {
				return ret;
			}
			offset += exist_len;
			entry.rec_len = cpu_to_le16(rec_len);
		}

		entry.inode = cpu_to_le32(inode->num);
		entry.name_len = name_len;
		entry.file_type = ext2_type_to_dirent(le16_to_cpu(inode->disk.i_mode));

		/* Write back the entry. */
		ret = ext2_dirent_write(dir, &entry, offset, name);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}

		/* Update inode link count. */
		inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) + 1);
		ext2_inode_flush(inode);
		return STATUS_SUCCESS;
	}

	/* Couldn't find a spare entry. Allocate a block for a new one. */
	for(i = 0; i < EXT2_NDIR_BLOCKS; i++) {
		if(le32_to_cpu(dir->disk.i_block[i]) != 0) {
			continue;
		}

		ret = ext2_block_alloc(dir->mount, false, &raw);
		if(ret != STATUS_SUCCESS) {
			return ret;
		}
		dir->disk.i_block[i] = cpu_to_le32(raw);

		entry.rec_len = cpu_to_le16(dir->mount->block_size);
		entry.inode = cpu_to_le32(inode->num);
		entry.name_len = name_len;
		entry.file_type = ext2_type_to_dirent(le16_to_cpu(inode->disk.i_mode));

		ext2_inode_resize(dir, dir->size + dir->mount->block_size);
		I_BLOCKS_INC(dir);
		ext2_inode_flush(dir);

		/* Write back the entry. */
		ret = ext2_dirent_write(dir, &entry, i * inode->mount->block_size, name);
		if(ret != STATUS_SUCCESS) {
			dir->disk.i_block[i] = 0;
			ext2_block_free(dir->mount, raw);
			return ret;
		}

		inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) + 1);
		ext2_inode_flush(inode);
		return STATUS_SUCCESS;
	}

	return STATUS_DIR_FULL;
}

/** Structure containing data for ext2_dir_remove(). */
typedef struct ext2_dir_remove {
	const char *name;		/**< Name of entry being removed. */
	offset_t previous;		/**< Offset of previous entry. */
	status_t ret;			/**< Status code. */
} ext2_dir_remove_t;

/** Iteration callback to remove an entry.
 * @param dir		Directory being iterated.
 * @param header	Pointer to directory entry header.
 * @param name		Name of entry.
 * @param offset	Offset of entry.
 * @param _data		Pointer to data structure.
 * @return		Whether to continue iterating. */
static bool ext2_dir_remove_cb(ext2_inode_t *dir, ext2_dirent_t *header, const char *name,
                               offset_t offset, void *_data) {
	ext2_dir_remove_t *data = _data;
	ext2_dirent_t prev;

	if(strcmp(name, data->name) != 0) {
		data->previous = offset;
		return true;
	}

	/* If this is not the first entry, we can resize the previous one. */
	if(offset) {
		data->ret = ext2_dirent_read(dir, &prev, data->previous, NULL);
		if(data->ret != STATUS_SUCCESS) {
			return false;
		}
		prev.rec_len = cpu_to_le16(le16_to_cpu(prev.rec_len) + le16_to_cpu(header->rec_len));
		data->ret = ext2_dirent_write(dir, &prev, data->previous, NULL);
		return false;
	} else {
		header->inode = 0;
		data->ret = ext2_dirent_write(dir, header, offset, NULL);
		return false;
	}
}

/** Remove an entry from a directory.
 * @note		Necessary locking should be done by caller.
 * @param dir		Directory to remove from.
 * @param name		Name of the entry to remove.
 * @param inode		Inode corresponding to the entry.
 * @return		Status code describing result of the operation. */
status_t ext2_dir_remove(ext2_inode_t *dir, const char *name, ext2_inode_t *inode) {
	ext2_dir_remove_t data = {
		.name = name,
		.previous = 0,
		.ret = STATUS_CORRUPT_FS,
	};
	status_t ret;

	ret = ext2_dir_iterate(dir, 0, ext2_dir_remove_cb, &data);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(data.ret != STATUS_SUCCESS) {
		return data.ret;
	}

	inode->disk.i_links_count = cpu_to_le16(le16_to_cpu(inode->disk.i_links_count) - 1);
	ext2_inode_flush(inode);
	return STATUS_SUCCESS;
}

/** Iteration callback for checking if a directory is empty.
 * @param dir		Directory being iterated.
 * @param header	Pointer to directory entry header.
 * @param name		Name of entry.
 * @param offset	Offset of entry.
 * @param data		Where to store whether directory is empty.
 * @return		Whether to continue iterating. */
static bool ext2_dir_empty_cb(ext2_inode_t *dir, ext2_dirent_t *header, const char *name,
                              offset_t offset, void *data) {
	if(strcmp(name, ".") != 0 && strcmp(name, "..") != 0) {
		*(bool *)data = false;
		return false;
	}
	return true;
}

/** Check whether an Ext2 directory is empty.
 * @note		Necessary locking should be done by caller.
 * @param dir		Directory to check.
 * @return		Whether the directory is empty. */
bool ext2_dir_empty(ext2_inode_t *dir) {
	bool empty = true;

	if(ext2_dir_iterate(dir, 0, ext2_dir_empty_cb, &empty) != STATUS_SUCCESS) {
		return false;
	}

	return empty;
}

/** Structure containing data for ext2_dir_lookup(). */
typedef struct ext2_dir_lookup {
	const char *name;		/**< Name of entry being looked up. */
	node_id_t *idp;			/**< Where to store ID of node. */
	bool found;			/**< Whether entry was found. */
} ext2_dir_lookup_t;

/** Iteration callback to look up an entry.
 * @param dir		Directory being iterated.
 * @param header	Pointer to directory entry header.
 * @param name		Name of entry.
 * @param offset	Offset of entry.
 * @param _data		Pointer to data structure.
 * @return		Whether to continue iterating. */
static bool ext2_dir_lookup_cb(ext2_inode_t *dir, ext2_dirent_t *header, const char *name,
                               offset_t offset, void *_data) {
	ext2_dir_lookup_t *data = _data;

	if(strcmp(name, data->name) == 0) {
		*data->idp = (node_id_t)le32_to_cpu(header->inode);
		data->found = true;
		return false;
	} else {
		return true;
	}
}

/** Look up an Ext2 directory entry.
 * @param cache		Cache to look up for.
 * @param name		Name of entry to look up.
 * @param idp		Where to store ID of node entry maps to.
 * @return		Status code describing result of the operation. */
static status_t ext2_dir_lookup(entry_cache_t *cache, const char *name, node_id_t *idp) {
	ext2_dir_lookup_t data = {
		.name = name,
		.idp = idp,
		.found = false,
	};
	status_t ret;

	ret = ext2_dir_iterate(cache->data, 0, ext2_dir_lookup_cb, &data);
	if(ret != STATUS_SUCCESS) {
		return ret;
	} else if(!data.found) {
		return STATUS_NOT_FOUND;
	}

	return STATUS_SUCCESS;
}

/** Ext2 entry cache operations structure. */
entry_cache_ops_t ext2_entry_cache_ops = {
	.lookup = ext2_dir_lookup,
};
