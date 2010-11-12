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

#ifndef __EXT2_PRIV_H
#define __EXT2_PRIV_H

#include <io/entry_cache.h>
#include <io/file_map.h>
#include <io/fs.h>

#include <mm/vm_cache.h>

#include <console.h>
#include <endian.h>

#include "ext2.h"

#if CONFIG_MODULE_FS_EXT2_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)		
#endif

/** Data for an Ext2 mount. */
typedef struct ext2_mount {
	mutex_t lock;			/**< Lock to protect filesystem structures. */

	ext2_superblock_t sb;		/**< Superblock of the filesystem. */
	ext2_group_desc_t *group_tbl;	/**< Pointer to block group descriptor table. */
	fs_mount_t *parent;		/**< Pointer to mount structure. */
	object_handle_t *device;	/**< Handle to backing device. */

	uint32_t revision;		/**< Filesystem revision. */
	uint32_t inodes_per_group;	/**< Inodes per group. */
	uint32_t inode_count;		/**< Inodes count. */
	uint32_t blocks_per_group;	/**< Blocks per group. */
	uint32_t block_count;		/**< Blocks count. */
	size_t block_size;		/**< Size of a block on the filesystem. */
	size_t block_groups;		/**< Number of block groups. */
	size_t inode_size;		/**< Size of an inode. */
	offset_t group_tbl_offset;	/**< Offset of the group table. */
	size_t group_tbl_size;		/**< Size of the group table. */
} ext2_mount_t;

/** In-memory node structure. */
typedef struct ext2_inode {
	mutex_t lock;			/**< Lock to protect the node (MUTEX_RECURSIVE). */

	vm_cache_t *cache;		/**< Data cache for the node. */
	entry_cache_t *entries;		/**< Directory entry cache. */
	file_map_t *map;		/**< Map of block numbers to raw blocks. */

	ext2_mount_t *mount;		/**< Pointer to mount data structure. */
	uint32_t num;			/**< Inode number. */
	uint64_t size;			/**< Size of data on disk. */

	ext2_disk_inode_t disk;		/**< On-disk inode structure. */
	size_t disk_size;		/**< Size of the inode structure on disk. */
	offset_t disk_offset;		/**< Offset into the device. */
} ext2_inode_t;

/** Callback function for ext2_dir_iterate().
 * @param dir		Directory being iterated.
 * @param header	Pointer to directory entry header.
 * @param name		Name of entry.
 * @param offset	Offset of entry.
 * @param data		Data argument passed to ext2_dir_iterate().
 * @return		True if should continue iterating, false if not. */
typedef bool (*ext2_dir_iterate_cb_t)(ext2_inode_t *dir, ext2_dirent_t *header,
                                      const char *name, offset_t offset,
                                      void *data);

/** Macros to increment/decrement i_blocks. */
#define I_BLOCKS_INC(i)			\
	((i)->disk.i_blocks = cpu_to_le32(le32_to_cpu((i)->disk.i_blocks) + ((i)->mount->block_size / 512)))
#define I_BLOCKS_DEC(i)			\
	((i)->disk.i_blocks = cpu_to_le32(le32_to_cpu((i)->disk.i_blocks) - ((i)->mount->block_size / 512)))

/** Convert an inode type to a directory entry type.
 * @param mode		Inode's mode value.
 * @return		Directory entry type code. */
static inline uint8_t ext2_type_to_dirent(uint16_t mode) {
	switch(mode & EXT2_S_IFMT) {
	case EXT2_S_IFSOCK:	return EXT2_FT_SOCK;
	case EXT2_S_IFLNK:	return EXT2_FT_SYMLINK;
	case EXT2_S_IFREG:	return EXT2_FT_REG_FILE;
	case EXT2_S_IFBLK:	return EXT2_FT_BLKDEV;
	case EXT2_S_IFDIR:	return EXT2_FT_DIR;
	case EXT2_S_IFCHR:	return EXT2_FT_CHRDEV;
	case EXT2_S_IFIFO:	return EXT2_FT_FIFO;
	default:		return EXT2_FT_UNKNOWN;
	}
}

extern entry_cache_ops_t ext2_entry_cache_ops;

extern status_t ext2_block_zero(ext2_mount_t *mount, uint32_t block);
extern status_t ext2_block_alloc(ext2_mount_t *mount, bool nonblock, uint32_t *blockp);
extern status_t ext2_block_free(ext2_mount_t *mount, uint32_t num);
extern status_t ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t block, bool nonblock);
extern status_t ext2_block_write(ext2_mount_t *mount, const void *buf, uint32_t block, bool nonblock);

extern status_t ext2_dir_iterate(ext2_inode_t *dir, offset_t index, ext2_dir_iterate_cb_t cb, void *data);
extern status_t ext2_dir_insert(ext2_inode_t *dir, const char *name, ext2_inode_t *inode);
extern status_t ext2_dir_remove(ext2_inode_t *dir, const char *name, ext2_inode_t *inode);
extern bool ext2_dir_empty(ext2_inode_t *dir);

extern status_t ext2_inode_alloc(ext2_mount_t *mount, uint16_t mode, const object_security_t *security,
                                 ext2_inode_t **inodep);
extern status_t ext2_inode_free(ext2_mount_t *mount, uint32_t num, uint16_t mode);
extern status_t ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **inodep);
extern status_t ext2_inode_flush(ext2_inode_t *inode);
extern void ext2_inode_release(ext2_inode_t *inode);
extern status_t ext2_inode_read(ext2_inode_t *inode, void *buf, size_t count, offset_t offset,
                                bool nonblock, size_t *bytesp);
extern status_t ext2_inode_write(ext2_inode_t *inode, const void *buf, size_t count, offset_t offset,
                                 bool nonblock, size_t *bytesp);
extern status_t ext2_inode_resize(ext2_inode_t *inode, offset_t size);
extern status_t ext2_inode_security(ext2_inode_t *inode, object_security_t **securityp);
extern status_t ext2_inode_set_security(ext2_inode_t *inode, const object_security_t *security);

extern void ext2_mount_flush(ext2_mount_t *mount);

#endif /* __EXT2_PRIV_H */
