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

#ifndef __EXT2_PRIV_H
#define __EXT2_PRIV_H

#include <io/device.h>
#include <io/vfs.h>

#include <sync/rwlock.h>

#include <console.h>

#if CONFIG_MODULE_FS_EXT2_DEBUG
# define dprintf(fmt...)	kprintf(LOG_DEBUG, fmt)
#else
# define dprintf(fmt...)		
#endif

/** EXT2 filesystem magic number. */
#define EXT2_MAGIC		0xEF53

/** Special block numbers. */
#define EXT2_NDIR_BLOCKS	12		/**< Direct blocks. */
#define EXT2_IND_BLOCK		12		/**< Indirect block. */
#define EXT2_DIND_BLOCK		13		/**< Double indirect block. */
#define EXT2_TIND_BLOCK		14		/**< Triple indirect block. */
#define EXT2_N_BLOCKS		15		/**< Total number of blocks. */

/** EXT2 revision numbers. */
#define EXT2_GOOD_OLD_REV	0
#define EXT2_DYNAMIC_REV	1

/** Filesystem status flags. */
#define EXT2_ERROR_FS		0
#define EXT2_VALID_FS		1

/** File type definitions. */
#define EXT2_S_IFMT		0xF000		/**< Format mask. */
#define EXT2_S_IFSOCK		0xC000		/**< Socket. */
#define EXT2_S_IFLNK		0xA000		/**< Symbolic link. */
#define EXT2_S_IFREG		0x8000		/**< Regular file. */
#define EXT2_S_IFBLK		0x6000		/**< Block device. */
#define EXT2_S_IFDIR		0x4000		/**< Directory. */
#define EXT2_S_IFCHR		0x2000		/**< Character device. */
#define EXT2_S_IFIFO		0x1000		/**< FIFO. */

/** Access rights. */
#define EXT2_S_ISUID		04000		/**< Set-UID. */
#define EXT2_S_ISGID		02000		/**< Set-GID. */
#define EXT2_S_ISVTX		01000		/**< Sticky bit. */
#define EXT2_S_IRWXU		00700		/**< User access mask. */
#define EXT2_S_IRUSR		00400		/**< User can read. */
#define EXT2_S_IWUSR		00200		/**< User can write. */
#define EXT2_S_IXUSR		00100		/**< User can execute. */
#define EXT2_S_IRWXG		00070		/**< Group access mask. */
#define EXT2_S_IRGRP		00040		/**< Group can read. */
#define EXT2_S_IWGRP		00020		/**< Group can write. */
#define EXT2_S_IXGRP		00010		/**< Group can execute. */
#define EXT2_S_IRWXO		00007		/**< Others access mask. */
#define EXT2_S_IROTH		00004		/**< Others can read. */
#define EXT2_S_IWOTH		00002		/**< Others can write. */
#define EXT2_S_IXOTH		00001		/**< Others can execute. */

/** File types in directory entries. */
#define EXT2_FT_UNKNOWN		0
#define EXT2_FT_REG_FILE	1
#define EXT2_FT_DIR		2
#define EXT2_FT_CHRDEV		3
#define EXT2_FT_BLKDEV		4
#define EXT2_FT_FIFO		5
#define EXT2_FT_SOCK		6
#define EXT2_FT_SYMLINK		7
#define EXT2_FT_MAX		8

/** Reserved inode numbers. */
#define EXT2_BAD_INO		0x01		/**< Bad blocks inode. */
#define EXT2_ROOT_INO		0x02		/**< Root directory inode. */
#define EXT2_ACL_IDX_INO	0x03		/**< ACL index inode. */
#define EXT2_ACL_DATA_IN	0x04		/**< ACL data inode. */
#define EXT2_BOOT_LOADER_INO	0x05		/**< Boot loader inode. */
#define EXT2_UNDEL_DIR_INO	0x06		/**< Undelete directory inode. */

/** Limitations. */
#define EXT2_NAME_MAX		256		/**< Maximum file name length. */

/** Superblock of an EXT2 filesystem. */
typedef struct ext2_superblock {
	uint32_t s_inodes_count;		/**< Inodes count. */
	uint32_t s_blocks_count;		/**< Blocks count. */
	uint32_t s_r_blocks_count;		/**< Reserved blocks count. */
	uint32_t s_free_blocks_count;		/**< Free blocks count. */
	uint32_t s_free_inodes_count;		/**< Free inodes count. */
	uint32_t s_first_data_block;		/**< First Data Block. */
	uint32_t s_log_block_size;		/**< Block size. */
	uint32_t s_log_frag_size;		/**< Fragment size. */
	uint32_t s_blocks_per_group;		/**< Number of blocks per group. */
	uint32_t s_frags_per_group;		/**< Number of fragments per group. */
	uint32_t s_inodes_per_group;		/**< Number of inodes per group. */
	uint32_t s_mtime;			/**< Mount time. */
	uint32_t s_wtime;			/**< Write time. */
	uint16_t s_mnt_count;			/**< Mount count. */
	uint16_t s_max_mnt_count;		/**< Maximal mount count. */
	uint16_t s_magic;			/**< Magic signature. */
	uint16_t s_state;			/**< File system state. */
	uint16_t s_errors;			/**< Behaviour when detecting errors. */
	uint16_t s_minor_rev_level;		/**< Minor revision level. */
	uint32_t s_lastcheck;			/**< Time of last check. */
	uint32_t s_checkinterval;		/**< Max time between checks. */
	uint32_t s_creator_os;			/**< OS. */
	uint32_t s_rev_level;			/**< Revision level. */
	uint16_t s_def_resuid;			/**< Default uid for reserved blocks. */
	uint16_t s_def_resgid;			/**< Default gid for reserved blocks. */

	/** EXT2_DYNAMIC_REV superblocks only. */
	uint32_t s_first_ino;			/**< First non-reserved inode. */
	uint16_t s_inode_size;			/**< Size of inode structure. */
	uint16_t s_block_group_nr;		/**< Block group number of this superblock. */
	uint32_t s_feature_compat;		/**< Compatible feature set. */
	uint32_t s_feature_incompat;		/**< Incompatible feature set. */
	uint32_t s_feature_ro_compat;		/**< Readonly-compatible feature set. */
	uint8_t  s_uuid[16];			/**< 128-bit uuid for volume. */
	char     s_volume_name[16];		/**< Volume name. */
	char     s_last_mounted[64];		/**< Directory where last mounted. */
	uint32_t s_algorithm_usage_bitmap;	/**< For compression. */

	/** Performance hints. */
	uint8_t  s_prealloc_blocks;		/**< Number of blocks to try to preallocate. */
	uint8_t  s_prealloc_dir_blocks;		/**< Number to preallocate for dirs. */
	uint16_t s_padding1;

	/** Journaling support (Ext3 compatibility). */
	uint8_t  s_journal_uuid[16];		/**< UUID of journal superblock. */
	uint32_t s_journal_inum;		/**< Inode number of journal file. */
	uint32_t s_journal_dev;			/**< Device number of journal file. */
	uint32_t s_last_orphan;			/**< Start of list of inodes to delete. */
	uint32_t s_hash_seed[4];		/**< HTREE hash seed. */
	uint8_t  s_def_hash_version;		/**< Default hash version to use. */
	uint8_t  s_jnl_backup_type;
	uint16_t s_reserved_word_pad;
	uint32_t s_default_mount_opts;
 	uint32_t s_first_meta_bg; 		/**< First metablock block group. */
	uint32_t s_mkfs_time;			/**< When the filesystem was created. */
	uint32_t s_jnl_blocks[17]; 		/**< Backup of the journal inode. */
	uint32_t s_reserved[172];		/**< Padding to the end of the block. */
} __attribute__((packed)) ext2_superblock_t;

/** Group descriptor table. */
typedef struct ext2_group_desc {
	uint32_t bg_block_bitmap;		/**< Blocks bitmap block. */
	uint32_t bg_inode_bitmap;		/**< Inode bitmap block. */
	uint32_t bg_inode_table;		/**< Inode table block. */
	uint16_t bg_free_blocks_count;		/**< Number of free blocks. */
	uint16_t bg_free_inodes_count;		/**< Number of free inodes. */
	uint16_t bg_used_dirs_count;		/**< Number of used directories. */
	uint16_t bg_pad;
	uint32_t bg_reserved[3];
} __attribute__((packed)) ext2_group_desc_t;

/** EXT2 inode structure. */
typedef struct ext2_disk_inode {
	uint16_t i_mode;			/**< File mode. */
	uint16_t i_uid;				/**< Lower 16-bits of owner's UID. */
	uint32_t i_size;			/**< File size. */
	uint32_t i_atime;			/**< Last access time. */
	uint32_t i_ctime;			/**< Creation time. */
	uint32_t i_mtime;			/**< Last modification time. */
	uint32_t i_dtime;			/**< Deletion time. */
	uint16_t i_gid;				/**< Lower 16-bits of owning GID. */
	uint16_t i_links_count;			/**< Number of links to this inode. */
	uint32_t i_blocks;			/**< Number of blocks. */
	uint32_t i_flags;			/**< File flags. */
	uint32_t i_osd1;			/**< OS-dependent data 1, we don't need this. */
	uint32_t i_block[EXT2_N_BLOCKS];	/**< Pointers to blocks. */
	uint32_t i_generation;			/**< File version (NFS). */
	uint32_t i_file_acl;			/**< File access control list. */
	uint32_t i_dir_acl;			/**< Directory access control list. */
	uint32_t i_faddr;			/**< Fragment address. */
	union {
		struct {
			uint8_t  l_i_frag;	/**< Fragment number. */
			uint8_t  l_i_fsize;	/**< Fragment size. */
			uint16_t l_i_pad1;
			uint16_t l_i_uid_high;	/**< Higher 16-bits of owner's UID. */
			uint16_t l_i_gid_high;	/**< Higher 16-bits of owning GID. */
			uint32_t l_i_reserved2;
		} linux2;
		struct {
			uint8_t  h_i_frag;	/**< Fragment number. */
			uint8_t  h_i_fsize;	/**< Fragment size. */
			uint16_t h_i_mode_high;
			uint16_t h_i_uid_high;
			uint16_t h_i_gid_high;
			uint32_t h_i_author;
		} hurd2;
		struct {
			uint8_t  m_i_frag;	/**< Fragment number. */
			uint8_t  m_i_fsize;	/**< Fragment size. */
			uint16_t m_i_pad1;
			uint32_t m_i_reserved2[2];
		} masix2;
	} osd2;					/**< OS-dependent data 2. */
} __attribute__((packed)) ext2_disk_inode_t;

/** EXT2 directory entry. */
typedef struct ext2_dirent {
	uint32_t inode;				/**< Inode number. */
	uint16_t rec_len;			/**< Length of the structure. */
	uint8_t name_len;			/**< Name length. */
	uint8_t file_type;			/**< File type. */
	char name[EXT2_NAME_MAX];		/**< Name of the file. */
} __attribute__((packed)) ext2_dirent_t;

/** Data for an Ext2 mount. */
typedef struct ext2_mount {
	rwlock_t lock;				/**< Lock to protect filesystem structures. */

	ext2_superblock_t sb;			/**< Superblock of the filesystem. */
	ext2_group_desc_t *group_tbl;		/**< Pointer to block group descriptor table. */
	vfs_mount_t *parent;			/**< Pointer to mount structure. */
	device_t *device;			/**< Pointer to backing device. */

	uint32_t inodes_per_group;		/**< Inodes per group. */
	uint32_t inodes_count;			/**< Inodes count. */
	uint32_t blocks_per_group;		/**< Blocks per group. */
	uint32_t blocks_count;			/**< Blocks count. */
	size_t blk_size;			/**< Size of a block on the filesystem. */
	size_t blk_groups;			/**< Number of block groups. */
	size_t in_size;				/**< Size of an inode. */
	offset_t group_tbl_off;			/**< Offset of the group table. */
	size_t group_tbl_size;			/**< Size of the group table. */
} ext2_mount_t;

/** In-memory inode structure. */
typedef struct ext2_inode {
	rwlock_t lock;				/**< Lock to protect the inode and its data. */
	uint32_t num;				/**< Inode number. */
	bool dirty;				/**< Whether the structure is dirty. */
	size_t size;				/**< Size of the inode structure on disk. */
	offset_t offset;			/**< Offset into the device. */
	ext2_mount_t *mount;			/**< Pointer to mount data structure. */
	ext2_disk_inode_t disk;			/**< On-disk inode structure. */
} ext2_inode_t;

/** Macros to increment/decrement i_blocks. */
#define I_BLOCKS_INC(i)			\
	((i)->disk.i_blocks = cpu_to_le32(le32_to_cpu((i)->disk.i_blocks) + ((i)->mount->blk_size / 512)))
#define I_BLOCKS_DEC(i)			\
	((i)->disk.i_blocks = cpu_to_le32(le32_to_cpu((i)->disk.i_blocks) - ((i)->mount->blk_size / 512)))

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

extern int ext2_block_alloc(ext2_mount_t *mount, bool nonblock, uint32_t *blockp);
extern int ext2_block_free(ext2_mount_t *mount, uint32_t num);
extern int ext2_block_read(ext2_mount_t *mount, void *buf, uint32_t block, bool nonblock);
extern int ext2_block_write(ext2_mount_t *mount, const void *buf, uint32_t block, bool nonblock);

extern int ext2_dir_cache(vfs_node_t *node);
extern int ext2_dir_insert(ext2_inode_t *dir, ext2_inode_t *inode, const char *name);
extern int ext2_dir_remove(ext2_inode_t *dir, ext2_inode_t *inode, const char *name);

extern int ext2_inode_alloc(ext2_mount_t *mount, uint16_t mode, ext2_inode_t **inodep);
extern int ext2_inode_free(ext2_mount_t *mount, uint32_t num, uint16_t mode);
extern int ext2_inode_get(ext2_mount_t *mount, uint32_t num, ext2_inode_t **inodep);
extern int ext2_inode_flush(ext2_inode_t *inode);
extern void ext2_inode_release(ext2_inode_t *inode);
extern int ext2_inode_read(ext2_inode_t *inode, void *buf, uint32_t block, size_t count, bool nonblock);
extern int ext2_inode_write(ext2_inode_t *inode, const void *buf, uint32_t block, size_t count, bool nonblock);
extern int ext2_inode_resize(ext2_inode_t *inode, file_size_t size);

extern void ext2_mount_flush(ext2_mount_t *mount);

#endif /* __EXT2_PRIV_H */
